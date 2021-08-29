/*
 * Copyright 2013 Canonical Ltd.
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
 */

#include <datetime/dbus-shared.h>
#include <datetime/timezone-timedated.h>

#include <gio/gio.h>

#include <cerrno>
#include <cstdlib>

namespace ayatana {
namespace indicator {
namespace datetime {

/***
****
***/

class TimedatedTimezone::Impl
{
public:

    Impl(TimedatedTimezone& owner, GDBusConnection* connection):
        m_owner{owner},
        m_connection{G_DBUS_CONNECTION(g_object_ref(G_OBJECT(connection)))},
        m_cancellable{g_cancellable_new()}
    {
        // set the fallback value
        m_owner.timezone.set("Etc/Utc");

        // watch for timedate1 on the bus
        m_watcher_id = g_bus_watch_name_on_connection(
            m_connection,
            Bus::Timedate1::BUSNAME,
            G_BUS_NAME_WATCHER_FLAGS_AUTO_START,
            on_timedate1_appeared,
            on_timedate1_vanished,
            this,
            nullptr);

        // listen for changed properties
        m_signal_subscription_id = g_dbus_connection_signal_subscribe(
            m_connection,
            Bus::Timedate1::IFACE,
            Bus::Properties::IFACE,
            Bus::Properties::Signals::PROPERTIES_CHANGED,
            Bus::Timedate1::ADDR,
            nullptr,
            G_DBUS_SIGNAL_FLAGS_NONE,
            on_properties_changed,
            this,
            nullptr);
    }

    ~Impl()
    {
        g_cancellable_cancel(m_cancellable);
        g_clear_object(&m_cancellable);

        g_bus_unwatch_name(m_watcher_id);

        g_dbus_connection_signal_unsubscribe(m_connection, m_signal_subscription_id);

        g_clear_object(&m_connection);
    }

private:

    static void on_timedate1_appeared(GDBusConnection * /*connection*/,
                                      const gchar     * name,
                                      const gchar     * /*name_owner*/,
                                      gpointer          gself)
    {
        g_debug("%s appeared on bus", name);

        static_cast<Impl*>(gself)->ask_for_timezone();
    }

    static void on_timedate1_vanished(GDBusConnection * /*connection*/,
                                      const gchar     * name,
                                      gpointer          /*gself*/)
    {
        g_debug("%s not present on bus", name);
    }

    static void on_properties_changed(GDBusConnection * /*connection*/,
                                      const gchar     * /*sender_name*/,
                                      const gchar     * /*object_path*/,
                                      const gchar     * /*interface_name*/,
                                      const gchar     * /*signal_name*/,
                                      GVariant        * parameters,
                                      gpointer          gself)
    {
        auto self = static_cast<Impl*>(gself);

        GVariant* changed_properties {};
        gchar** invalidated_properties {};
        g_variant_get(parameters, "(s@a{sv}^as)", NULL, &changed_properties, &invalidated_properties);

        const char* tz {};
        if (g_variant_lookup(changed_properties, Bus::Timedate1::Properties::TIMEZONE, "&s", &tz, NULL))
        {
            if (tz != nullptr)
                self->set_timezone(tz);
            else
                g_warning("%s no timezone found", G_STRLOC);
        }
        else if (g_strv_contains(invalidated_properties, Bus::Timedate1::Properties::TIMEZONE))
        {
            self->ask_for_timezone();
        }

        g_variant_unref(changed_properties);
        g_strfreev(invalidated_properties);
    }

    void ask_for_timezone()
    {
        g_dbus_connection_call(
            m_connection,
            Bus::Timedate1::BUSNAME,
            Bus::Timedate1::ADDR,
            Bus::Properties::IFACE,
            Bus::Properties::Methods::GET,
            g_variant_new("(ss)", Bus::Timedate1::IFACE, Bus::Timedate1::Properties::TIMEZONE),
            G_VARIANT_TYPE("(v)"),
            G_DBUS_CALL_FLAGS_NONE,
            -1,
            m_cancellable,
            on_get_timezone_ready,
            this);
    }

    static void on_get_timezone_ready(GObject       * connection,
                                      GAsyncResult  * res,
                                      gpointer        gself)
    {
        GError* error {};
        GVariant* v = g_dbus_connection_call_finish(G_DBUS_CONNECTION(connection), res, &error);
        if (error != nullptr)
        {
            if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                g_warning("%s Couldn't get timezone: %s", G_STRLOC, error->message);
        }
        else if (v != nullptr)
        {
            GVariant* tzv {};
            g_variant_get(v, "(v)", &tzv);
            const char* tz = g_variant_get_string(tzv, nullptr);

            if (tz != nullptr)
                static_cast<Impl*>(gself)->set_timezone(tz);
            else
                g_warning("%s no timezone found", G_STRLOC);

            g_clear_pointer(&tzv, g_variant_unref);
            g_clear_pointer(&v, g_variant_unref);
        }
    }

    void set_timezone(const std::string& tz)
    {
        g_return_if_fail(!tz.empty());

        g_debug("set timezone: '%s'", tz.c_str());
        m_owner.timezone.set(tz);
    }

    /***
    ****
    ***/

    TimedatedTimezone& m_owner;
    GDBusConnection* m_connection {};
    GCancellable* m_cancellable {};
    unsigned long m_signal_subscription_id {};
    unsigned int m_watcher_id {};
};

/***
****
***/

TimedatedTimezone::TimedatedTimezone(GDBusConnection* connection):
    impl{new Impl{*this, connection}}
{
}

TimedatedTimezone::~TimedatedTimezone()
{
}

/***
****
***/

} // namespace datetime
} // namespace indicator
} // namespace ayatana
