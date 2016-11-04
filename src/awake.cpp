/*
 * Copyright 2014 Canonical Ltd.
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

#include <notifications/awake.h>
#include <notifications/dbus-shared.h>

#include <gio/gio.h>
#include <string>
#include <limits>

namespace ayatana {
namespace indicator {
namespace notifications {

/***
****
***/

class Awake::Impl
{
public:

    Impl(GDBusConnection* bus, const std::string& app_name):
        m_app_name(app_name),
        m_cancellable(g_cancellable_new()),
        m_system_bus{G_DBUS_CONNECTION(g_object_ref(bus))}
    {
        // ask repowerd to keep the system awake
        static constexpr int32_t POWERD_SYS_STATE_ACTIVE = 1;
        g_dbus_connection_call (m_system_bus,
                                BUS_POWERD_NAME,
                                BUS_POWERD_PATH,
                                BUS_POWERD_INTERFACE,
                                "requestSysState",
                                g_variant_new("(si)", m_app_name.c_str(), POWERD_SYS_STATE_ACTIVE),
                                G_VARIANT_TYPE("(s)"),
                                G_DBUS_CALL_FLAGS_NONE,
                                -1,
                                m_cancellable,
                                on_force_awake_response,
                                this);

    }

    ~Impl()
    {
        g_cancellable_cancel (m_cancellable);
        g_object_unref (m_cancellable);
        unforce_awake ();
        g_clear_object (&m_system_bus);
    }

private:

    static void on_force_awake_response (GObject      * connection,
                                         GAsyncResult * res,
                                         gpointer       gself)
    {
        GError * error;
        GVariant * args;

        error = nullptr;
        args = g_dbus_connection_call_finish (G_DBUS_CONNECTION(connection),
                                              res,
                                              &error);
        if (error != nullptr)
        {
            if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED) &&
                !g_error_matches (error, G_DBUS_ERROR, G_DBUS_ERROR_SERVICE_UNKNOWN))
            {
                g_warning ("Unable to inhibit sleep: %s", error->message);
            }

            g_error_free (error);
        }
        else
        {
            auto self = static_cast<Impl*>(gself);

            g_clear_pointer (&self->m_awake_cookie, g_free);
            g_variant_get (args, "(s)", &self->m_awake_cookie);

            g_variant_unref (args);
        }
    }

    void unforce_awake ()
    {
        g_return_if_fail (G_IS_DBUS_CONNECTION(m_system_bus));

        if (m_awake_cookie != nullptr)
        {
            g_dbus_connection_call (m_system_bus,
                                    BUS_POWERD_NAME,
                                    BUS_POWERD_PATH,
                                    BUS_POWERD_INTERFACE,
                                    "clearSysState",
                                    g_variant_new("(s)", m_awake_cookie),
                                    nullptr,
                                    G_DBUS_CALL_FLAGS_NONE,
                                    -1,
                                    m_cancellable,
                                    nullptr,
                                    nullptr);

            g_clear_pointer (&m_awake_cookie, g_free);
        }
    }

    const std::string m_app_name;
    GCancellable * m_cancellable = nullptr;
    GDBusConnection * m_system_bus = nullptr;
    char * m_awake_cookie = nullptr;
};

/***
****
***/

Awake::Awake(GDBusConnection* system_bus, const std::string& app_name):
    impl{new Impl{system_bus, app_name}}
{
}

Awake::~Awake()
{
}

/***
****
***/

} // namespace datetime
} // namespace indicator
} // namespace ayatana
