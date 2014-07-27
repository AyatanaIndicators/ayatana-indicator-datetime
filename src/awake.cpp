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

#include <limits>

namespace unity {
namespace indicator {
namespace notifications {

/***
****
***/

class Awake::Impl
{
public:

    Impl(const std::string& app_name):
        m_app_name(app_name),
        m_cancellable(g_cancellable_new())
    {
        g_bus_get(G_BUS_TYPE_SYSTEM, m_cancellable, on_system_bus_ready, this);
    }

    ~Impl()
    {
        g_cancellable_cancel (m_cancellable);
        g_object_unref (m_cancellable);

        if (m_system_bus != nullptr)
        {
            unforce_awake ();
            unforce_screen ();
            g_object_unref (m_system_bus);
        } 
    }

private:

    static void on_system_bus_ready (GObject *, GAsyncResult *res, gpointer gself)
    {
        GError * error;
        GDBusConnection * system_bus;

        error = nullptr;
        system_bus = g_bus_get_finish (res, &error);
        if (error != nullptr)
        {
            if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                g_warning ("Unable to get bus: %s", error->message);

            g_error_free (error);
        }
        else if (system_bus != nullptr)
        {
            auto self = static_cast<Impl*>(gself);

            self->m_system_bus = G_DBUS_CONNECTION (g_object_ref (system_bus));

            // ask powerd to keep the system awake
            static constexpr int32_t POWERD_SYS_STATE_ACTIVE = 1;
            g_dbus_connection_call (system_bus,
                                    BUS_POWERD_NAME,
                                    BUS_POWERD_PATH,
                                    BUS_POWERD_INTERFACE,
                                    "requestSysState",
                                    g_variant_new("(si)", self->m_app_name.c_str(), POWERD_SYS_STATE_ACTIVE),
                                    G_VARIANT_TYPE("(s)"),
                                    G_DBUS_CALL_FLAGS_NONE,
                                    -1,
                                    self->m_cancellable,
                                    on_force_awake_response,
                                    self);

            // ask unity-system-compositor to turn on the screen
            g_dbus_connection_call (system_bus,
                                    BUS_SCREEN_NAME,
                                    BUS_SCREEN_PATH,
                                    BUS_SCREEN_INTERFACE,
                                    "keepDisplayOn",
                                    nullptr,
                                    G_VARIANT_TYPE("(i)"),
                                    G_DBUS_CALL_FLAGS_NONE,
                                    -1,
                                    self->m_cancellable,
                                    on_force_screen_response,
                                    self);

            g_object_unref (system_bus);
        }
    }

    static void on_force_awake_response (GObject      * connection,
                                         GAsyncResult * res,
                                         gpointer       gself)
    {
        GError * error;
        GVariant * args;

        error = nullptr;
        args = g_dbus_connection_call_finish (G_DBUS_CONNECTION(connection), res, &error);
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
            g_debug ("m_awake_cookie is now '%s'", self->m_awake_cookie);
 
            g_variant_unref (args);
        }
    }

    static void on_force_screen_response (GObject      * connection,
                                          GAsyncResult * res,
                                          gpointer       gself)
    {
        GError * error;
        GVariant * args;

        error = nullptr;
        args = g_dbus_connection_call_finish (G_DBUS_CONNECTION(connection), res, &error);
        if (error != nullptr)
        {
            if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED) &&
                !g_error_matches (error, G_DBUS_ERROR, G_DBUS_ERROR_SERVICE_UNKNOWN))
            {
                g_warning ("Unable to turn on the screen: %s", error->message);
            }

            g_error_free (error);
        }
        else
        {
            auto self = static_cast<Impl*>(gself);

            self->m_screen_cookie = NO_SCREEN_COOKIE;
            g_variant_get (args, "(i)", &self->m_screen_cookie);
            g_debug ("m_screen_cookie is now '%d'", self->m_screen_cookie);
 
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
                                    nullptr,
                                    nullptr,
                                    nullptr);

            g_clear_pointer (&m_awake_cookie, g_free);
        }
    }

    void unforce_screen ()
    {
        g_return_if_fail (G_IS_DBUS_CONNECTION(m_system_bus));

        if (m_screen_cookie != NO_SCREEN_COOKIE)
        {
            g_dbus_connection_call (m_system_bus,
                                    BUS_SCREEN_NAME,
                                    BUS_SCREEN_PATH,
                                    BUS_SCREEN_INTERFACE,
                                    "removeDisplayOnRequest",
                                    g_variant_new("(i)", m_screen_cookie),
                                    nullptr,
                                    G_DBUS_CALL_FLAGS_NONE,
                                    -1,
                                    nullptr,
                                    nullptr,
                                    nullptr);

            m_screen_cookie = NO_SCREEN_COOKIE;
        }
    }

    const std::string m_app_name;
    GCancellable * m_cancellable = nullptr;
    GDBusConnection * m_system_bus = nullptr;
    char * m_awake_cookie = nullptr;
    int32_t m_screen_cookie = NO_SCREEN_COOKIE;

    static constexpr int32_t NO_SCREEN_COOKIE { std::numeric_limits<int32_t>::min() };
};

/***
****
***/

Awake::Awake(const std::string& app_name):
  impl(new Impl (app_name))
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
} // namespace unity
