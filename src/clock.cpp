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

#include <glib.h>
#include <gio/gio.h>

#include <map>
#include <set>
#include <string>
#include <vector>

namespace unity {
namespace indicator {
namespace datetime {

/***
****
***/

class Clock::Impl
{
public:

    Impl(Clock& owner):
        m_owner(owner)
    {
        auto tag = g_bus_watch_name(G_BUS_TYPE_SYSTEM,
                                    "org.freedesktop.login1",
                                    G_BUS_NAME_WATCHER_FLAGS_NONE,
                                    on_login1_appeared,
                                    on_login1_vanished,
                                    this, nullptr);
        m_watched_names.insert(tag);
    }


    ~Impl()
    {
        for(const auto& tag : m_watched_names)
            g_bus_unwatch_name(tag);
    }

private:

    void remember_subscription(const std::string& name,
                               GDBusConnection* bus,
                               guint tag)
    {
        auto subscription = std::shared_ptr<GDBusConnection>(
              G_DBUS_CONNECTION(g_object_ref(bus)),
              [tag](GDBusConnection* bus){
                  g_dbus_connection_signal_unsubscribe(bus, tag);
                  g_object_unref(G_OBJECT(bus));
              }
        );

        m_subscriptions[name].push_back(subscription);
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
        static_cast<Impl*>(gself)->m_owner.minute_changed();
    }

    /***
    ****
    ***/

    Clock& m_owner;
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
} // namespace unity
