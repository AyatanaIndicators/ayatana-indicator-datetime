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

#include <datetime/clock.h>
#include <datetime/dbus-shared.h>

#include <glib.h>
#include <gio/gio.h>

#include <map>
#include <set>
#include <string>
#include <vector>

namespace ayatana {
namespace indicator {
namespace datetime {

/***
****
***/

class Clock::Impl
{
public:

    Impl(Clock& owner):
        m_owner(owner),
        m_cancellable(g_cancellable_new())
    {
        g_bus_get(G_BUS_TYPE_SYSTEM, m_cancellable, on_bus_ready, this);
    }

    ~Impl()
    {
        g_cancellable_cancel(m_cancellable);
        g_object_unref(m_cancellable);

        for(const auto& tag : m_watched_names)
            g_bus_unwatch_name(tag);
    }

private:

    static void on_bus_ready(GObject      * /*source_object*/,
                             GAsyncResult * res,
                             gpointer       gself)
    {
        GError * error = NULL;
        GDBusConnection * bus;

        if ((bus = g_bus_get_finish(res, &error)))
        {
            auto self = static_cast<Impl*>(gself);

            auto tag = g_bus_watch_name_on_connection(bus,
                                                      "org.freedesktop.login1",
                                                      G_BUS_NAME_WATCHER_FLAGS_NONE,
                                                      on_login1_appeared,
                                                      on_login1_vanished,
                                                      gself, nullptr);
            self->m_watched_names.insert(tag);

            tag = g_bus_watch_name_on_connection(bus,
                                                 BUS_POWERD_NAME,
                                                 G_BUS_NAME_WATCHER_FLAGS_NONE,
                                                 on_powerd_appeared,
                                                 on_powerd_vanished,
                                                 gself, nullptr);
            self->m_watched_names.insert(tag);

            g_object_unref(bus);
        }
        else if (error != nullptr)
        {
            if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                g_warning("%s Couldn't get system bus: %s", G_STRLOC, error->message);

            g_error_free(error);
        }
    }

    void remember_subscription(const std::string  & name,
                               GDBusConnection    * bus,
                               guint                tag)
    {
        g_object_ref(bus);

        auto deleter = [tag](GDBusConnection* bus){
            g_dbus_connection_signal_unsubscribe(bus, tag);
            g_object_unref(G_OBJECT(bus));
        };

        m_subscriptions[name].push_back(std::shared_ptr<GDBusConnection>(bus, deleter));
    }

    /**
    ***  DBus Chatter: org.freedesktop.login1
    ***
    ***  Fire Clock::minute_changed() signal on login1's PrepareForSleep signal
    **/

    static void on_login1_appeared(GDBusConnection * bus,
                                   const gchar     * name,
                                   const gchar     * name_owner,
                                   gpointer          gself)
    {
        auto tag = g_dbus_connection_signal_subscribe(bus,
                                                      name_owner,
                                                      "org.freedesktop.login1.Manager", // interface
                                                      "PrepareForSleep", // signal name
                                                      "/org/freedesktop/login1", // object path
                                                      nullptr, // arg0
                                                      G_DBUS_SIGNAL_FLAGS_NONE,
                                                      on_prepare_for_sleep,
                                                      gself,
                                                      nullptr);

        static_cast<Impl*>(gself)->remember_subscription(name, bus, tag);
    }

    static void on_login1_vanished(GDBusConnection * /*system_bus*/,
                                   const gchar     * name,
                                   gpointer          gself)
    {
      static_cast<Impl*>(gself)->m_subscriptions[name].clear();
    }

    static void on_prepare_for_sleep(GDBusConnection* /*connection*/,
                                     const gchar*     /*sender_name*/,
                                     const gchar*     /*object_path*/,
                                     const gchar*     /*interface_name*/,
                                     const gchar*     /*signal_name*/,
                                     GVariant*        /*parameters*/,
                                     gpointer           gself)
    {
        g_debug("firing clock.minute_changed() due to PrepareForSleep");
        static_cast<Impl*>(gself)->m_owner.minute_changed();
    }

    /**
    ***  DBus Chatter: com.canonical.powerd
    ***
    ***  Fire Clock::minute_changed() signal when powerd says the system's
    ***  has awoken from sleep -- the old timestamp is likely out-of-date
    **/

    static void on_powerd_appeared(GDBusConnection * bus,
                                   const gchar     * name,
                                   const gchar     * name_owner,
                                   gpointer          gself)
    {
        auto tag = g_dbus_connection_signal_subscribe(bus,
                                                      name_owner,
                                                      BUS_POWERD_INTERFACE,
                                                      "SysPowerStateChange",
                                                      BUS_POWERD_PATH,
                                                      nullptr, // arg0
                                                      G_DBUS_SIGNAL_FLAGS_NONE,
                                                      on_sys_power_state_change,
                                                      gself, // user_data
                                                      nullptr); // user_data closure


        static_cast<Impl*>(gself)->remember_subscription(name, bus, tag);
    }

    static void on_powerd_vanished(GDBusConnection * /*bus*/,
                                   const gchar     * name,
                                   gpointer          gself)
    {
        static_cast<Impl*>(gself)->m_subscriptions[name].clear();
    }

    static void on_sys_power_state_change(GDBusConnection* /*connection*/,
                                            const gchar*     /*sender_name*/,
                                            const gchar*     /*object_path*/,
                                            const gchar*     /*interface_name*/,
                                            const gchar*     /*signal_name*/,
                                            GVariant*        /*parameters*/,
                                            gpointer           gself)
    {
        g_debug("firing clock.minute_changed() due to state change");
        static_cast<Impl*>(gself)->m_owner.minute_changed();
    }

    /***
    ****
    ***/

    Clock& m_owner;
    GCancellable * m_cancellable = nullptr;
    std::set<guint> m_watched_names;
    std::map<std::string,std::vector<std::shared_ptr<GDBusConnection>>> m_subscriptions;
};

/***
****
***/

Clock::Clock():
   m_impl(new Impl{*this})
{
}

Clock::~Clock()
{
}

/***
****
***/

} // namespace datetime
} // namespace indicator
} // namespace ayatana
