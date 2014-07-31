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

#include <notifications/dbus-shared.h>
#include <notifications/haptic.h>

#include <gio/gio.h>

namespace unity {
namespace indicator {
namespace notifications {

/***
****
***/

class Haptic::Impl
{
public:

    Impl(const Mode& mode):
        m_mode(mode),
        m_cancellable(g_cancellable_new())
    {
        g_bus_get (G_BUS_TYPE_SESSION, m_cancellable, on_bus_ready, this);
    }

    ~Impl()
    {
        if (m_tag)
            g_source_remove(m_tag);

        g_cancellable_cancel (m_cancellable);
        g_object_unref (m_cancellable);

        g_clear_object (&m_bus);
    }

private:

    static void on_bus_ready (GObject*, GAsyncResult* res, gpointer gself)
    {
        GError * error;
        GDBusConnection * bus;

        error = nullptr;
        bus = g_bus_get_finish (res, &error);
        if (error != nullptr)
        {
            if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                g_warning ("Unable to get bus: %s", error->message);

            g_error_free (error);
        }
        else if (bus != nullptr)
        {
            auto self = static_cast<Impl*>(gself);

            self->m_bus = G_DBUS_CONNECTION (g_object_ref (bus));
            self->start_vibrating();

            g_object_unref (bus);
        }
    }

    void start_vibrating()
    {
        /* We only support one vibrate mode for now: an on/off pulse at
           one-second intervals. So set a looping 2-second timer that asks
           the phone to vibrate for one second. */
        m_tag = g_timeout_add_seconds (2, on_timeout, this);
        on_timeout (this);
    }

    static gboolean on_timeout (gpointer gself)
    {
        static_cast<Impl*>(gself)->vibrate_now();
        return G_SOURCE_CONTINUE;
    }

    void vibrate_now()
    {
        g_dbus_connection_call (m_bus,
                                BUS_HAPTIC_NAME,
                                BUS_HAPTIC_PATH,
                                BUS_HAPTIC_INTERFACE,
                                "Vibrate",
                                g_variant_new("(u)", 1000u),
                                nullptr,
                                G_DBUS_CALL_FLAGS_NONE,
                                -1,
                                m_cancellable,
                                nullptr,
                                nullptr);
    }

    const Mode m_mode;
    GCancellable * m_cancellable = nullptr;
    GDBusConnection * m_bus = nullptr;
    guint m_tag = 0;
};

/***
****
***/

Haptic::Haptic(const Mode& mode):
    impl(new Impl (mode))
{
}

Haptic::~Haptic()
{
}

/***
****
***/

} // namespace datetime
} // namespace indicator
} // namespace unity
