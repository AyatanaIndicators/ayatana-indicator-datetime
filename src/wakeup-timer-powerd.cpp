/*
 * Copyright 2014 Canonical Ltd.
 * Copyright 2021-2022 Robert Tari
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3, as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranties of
 * MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Charles Kerr <charles.kerr@canonical.com>
 *   Robert Tari <robert@tari.in>
 */

#include <datetime/clock.h>
#include <datetime/wakeup-timer-powerd.h>

#include <notifications/dbus-shared.h> // BUS_POWERD_NAME

#include <gio/gio.h>

#include <memory> // std::shared_ptr

namespace ayatana {
namespace indicator {
namespace datetime {

/***
****
***/

class PowerdWakeupTimer::Impl
{
public:

    explicit Impl(const std::shared_ptr<Clock>& clock):
        m_cancellable(g_cancellable_new())
    {
        g_bus_get(G_BUS_TYPE_SYSTEM, m_cancellable, on_bus_ready, this);
    }

    ~Impl()
    {
        clear_current_cookie();

        g_cancellable_cancel(m_cancellable);
        g_clear_object(&m_cancellable);

        if (m_sub_id)
            g_dbus_connection_signal_unsubscribe(m_bus.get(), m_sub_id);

        if (m_watch_tag)
            g_bus_unwatch_name(m_watch_tag);
    }

    void set_wakeup_time(const DateTime& d)
    {
        m_wakeup_time = d;
        update_cookie();
    }

    core::Signal<>& timeout() { return m_timeout; }

private:

    void emit_timeout() { return m_timeout(); }

    static void on_bus_ready(GObject      * /*unused*/,
                             GAsyncResult * res,
                             gpointer       gself)
    {
        GError * error;
        GDBusConnection * bus;

        error = nullptr;
        bus = g_bus_get_finish(res, &error);
        if (bus == nullptr)
        {
            if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                g_warning("%s Couldn't get system bus: %s", G_STRLOC, error->message);
        }
        else
        {
            static_cast<Impl*>(gself)->init_bus(bus);
        }

        g_clear_object(&bus);
        g_clear_error(&error);
    }

    void init_bus(GDBusConnection* connection)
    {
        m_bus.reset(G_DBUS_CONNECTION(g_object_ref(G_OBJECT(connection))),
                    [](GDBusConnection *c){g_object_unref(G_OBJECT(c));});

        m_sub_id = g_dbus_connection_signal_subscribe(m_bus.get(),
                                                      BUS_POWERD_NAME,
                                                      BUS_POWERD_INTERFACE,
                                                      "Wakeup",
                                                      BUS_POWERD_PATH,
                                                      nullptr,
                                                      G_DBUS_SIGNAL_FLAGS_NONE,
                                                      on_wakeup_signal,
                                                      this, // userdata
                                                      nullptr); // userdata free

        m_watch_tag = g_bus_watch_name_on_connection(m_bus.get(),
                                                     BUS_POWERD_NAME,
                                                     G_BUS_NAME_WATCHER_FLAGS_NONE,
                                                     on_name_appeared_static,
                                                     nullptr, // name-vanished,
                                                     this, // userdata
                                                     nullptr); // userdata free
    }

    static void
    on_wakeup_signal(GDBusConnection * /*connection*/,
                     const gchar     * sender_name,
                     const gchar     * /*object_path*/,
                     const gchar     * /*interface_name*/,
                     const gchar     * /*signal_name*/,
                     GVariant        * /*parameters*/,
                     gpointer          gself)
    {
        g_debug("%s %s broadcast a hw wakeup signal", G_STRLOC, sender_name);
        static_cast<Impl*>(gself)->emit_timeout();
    }

    static void
    on_name_appeared_static(GDBusConnection * /*connection*/,
                            const gchar     * name,
                            const gchar     * name_owner,
                            gpointer          gself)
    {
        g_debug("%s %s owns %s now; let's ask for a new cookie", G_STRLOC, name, name_owner);
        static_cast<Impl*>(gself)->update_cookie();
    }

    /***
    ****  requestWakeup
    ***/

    void update_cookie()
    {
        if (!m_bus)
            return;

        // if we've already got a cookie, clear it
        clear_current_cookie();
        g_warn_if_fail(m_cookie.empty());

        // get a new cookie, if necessary
        if (m_wakeup_time.is_set())
        {
            g_debug("%s calling %s::requestWakeup(%s)",
                    G_STRLOC, BUS_POWERD_NAME,
                    m_wakeup_time.format("%F %T").c_str());

            auto args = g_variant_new("(st)",
                                      GETTEXT_PACKAGE,
                                      uint64_t(m_wakeup_time.to_unix()));

            g_dbus_connection_call(m_bus.get(),
                                   BUS_POWERD_NAME,
                                   BUS_POWERD_PATH,
                                   BUS_POWERD_INTERFACE,
                                   "requestWakeup", // method_name
                                   args,
                                   G_VARIANT_TYPE("(s)"), // reply_type
                                   G_DBUS_CALL_FLAGS_NONE,
                                   -1, // use default timeout
                                   m_cancellable,
                                   on_request_wakeup_done,
                                   this);
        }
    }

    static void on_request_wakeup_done(GObject      * o,
                                       GAsyncResult * res,
                                       gpointer       gself)
    {
        GError * error;
        GVariant * ret;

        error = nullptr;
        ret = g_dbus_connection_call_finish(G_DBUS_CONNECTION(o), res, &error);
        if (ret == nullptr)
        {
            /* powerd isn't on the desktop, but we don't need hardware wakeups there
               anyway... so no need to warn on SERVICE_UNKNOWN */
            if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED) &&
                !g_error_matches(error, G_DBUS_ERROR, G_DBUS_ERROR_SERVICE_UNKNOWN))
            {
                g_warning("%s Could not set hardware wakeup: %s", G_STRLOC, error->message);
            }
        }
        else
        {
            const char* s = nullptr;
            g_variant_get(ret, "(&s)", &s);
            g_debug("%s %s::requestWakeup() sent cookie %s",
                    G_STRLOC, BUS_POWERD_NAME, s);

            auto& cookie = static_cast<Impl*>(gself)->m_cookie;
            if (s != nullptr)
                cookie = s;
            else
                cookie.clear();
        }

        // cleanup
        g_clear_pointer(&ret, g_variant_unref);
        g_clear_error(&error);
    }

    /***
    ****  clearWakeup
    ***/

    void clear_current_cookie()
    {
        if (!m_cookie.empty())
        {
            g_debug("%s calling %s::clearWakeup(%s)",
                    G_STRLOC, BUS_POWERD_NAME, m_cookie.c_str());

            g_dbus_connection_call(m_bus.get(),
                                   BUS_POWERD_NAME,
                                   BUS_POWERD_PATH,
                                   BUS_POWERD_INTERFACE,
                                   "clearWakeup", // method_name
                                   g_variant_new("(s)", m_cookie.c_str()),
                                   nullptr, // no response type
                                   G_DBUS_CALL_FLAGS_NONE,
                                   -1, // use default timeout
                                   nullptr, // cancellable
                                   on_clear_wakeup_done,
                                   nullptr);
            m_cookie.clear();
        }
    }

    // this is only here to log errors
    static void on_clear_wakeup_done(GObject      * o,
                                     GAsyncResult * res,
                                     gpointer       /*unused*/)
    {
        GError * error;
        GVariant * ret;

        error = nullptr;
        ret = g_dbus_connection_call_finish(G_DBUS_CONNECTION(o), res, &error);
        if (!ret && !g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
            g_warning("%s Couldn't clear hardware wakeup: %s", G_STRLOC, error->message);

        // cleanup
        g_clear_pointer(&ret, g_variant_unref);
        g_clear_error(&error);
    }

    /***
    ****
    ***/

    core::Signal<> m_timeout;
    DateTime m_wakeup_time;

    std::shared_ptr<GDBusConnection> m_bus;
    GCancellable * m_cancellable = nullptr;
    std::string m_cookie;
    guint m_watch_tag = 0;
    guint m_sub_id = 0;
};

/***
****
***/

PowerdWakeupTimer::PowerdWakeupTimer(const std::shared_ptr<Clock>& clock):
    p(new Impl(clock))
{
}

PowerdWakeupTimer::~PowerdWakeupTimer()
{
}

void PowerdWakeupTimer::set_wakeup_time(const DateTime& d)
{
    p->set_wakeup_time(d);
}

core::Signal<>& PowerdWakeupTimer::timeout()
{
    return p->timeout();
}

/***
****
***/

} // namespace datetime
} // namespace indicator
} // namespace ayatana
