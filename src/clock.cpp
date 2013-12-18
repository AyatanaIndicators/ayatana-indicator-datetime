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

namespace unity {
namespace indicator {
namespace datetime {

/***
****
***/

Clock::Clock():
    cancellable_(g_cancellable_new())
{
    g_bus_get(G_BUS_TYPE_SYSTEM, cancellable_, onSystemBusReady, this);

    timezones.changed().connect([this](const std::set<std::string>& timezones){
        g_message ("timezones changed... new count is %d", (int)timezones.size());
        skewDetected();
    });
}

Clock::~Clock()
{
    g_cancellable_cancel(cancellable_);
    g_clear_object(&cancellable_);

    if (sleep_subscription_id_)
        g_dbus_connection_signal_unsubscribe(system_bus_ , sleep_subscription_id_);

    g_clear_object(&system_bus_);
}

void
Clock::onSystemBusReady(GObject*, GAsyncResult * res, gpointer gself)
{
    GDBusConnection * system_bus;

    if ((system_bus = g_bus_get_finish(res, nullptr)))
    {
        auto self = static_cast<Clock*>(gself);

        self->system_bus_ = system_bus;

        self->sleep_subscription_id_ = g_dbus_connection_signal_subscribe(
                        system_bus,
                        nullptr,
                        "org.freedesktop.login1.Manager", // interface
                        "PrepareForSleep", // signal name
                        "/org/freedesktop/login1", // object path
                        nullptr, // arg0
                        G_DBUS_SIGNAL_FLAGS_NONE,
                        onPrepareForSleep,
                        self,
                        nullptr);
    }
}

void
Clock::onPrepareForSleep(GDBusConnection * connection      G_GNUC_UNUSED,
                         const gchar     * sender_name     G_GNUC_UNUSED,
                         const gchar     * object_path     G_GNUC_UNUSED,
                         const gchar     * interface_name  G_GNUC_UNUSED,
                         const gchar     * signal_name     G_GNUC_UNUSED,
                         GVariant        * parameters      G_GNUC_UNUSED,
                         gpointer          gself)
{
    static_cast<Clock*>(gself)->skewDetected();
}

/***
****
***/

} // namespace datetime
} // namespace indicator
} // namespace unity
