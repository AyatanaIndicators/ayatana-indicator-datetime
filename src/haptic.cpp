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

#include <notifications/haptic.h>

#include <ubuntu/application/sensors/haptic.h>

#include <glib.h>

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
        m_sensor(ua_sensors_haptic_new())
    {
        if (m_sensor == nullptr)
        {
            g_warning ("Haptic device unavailable");
        }
        else
        {
            ua_sensors_haptic_enable(m_sensor);
            m_tag = g_timeout_add_seconds (2, on_timeout, this);
            on_timeout (this);
        }
    }

    ~Impl()
    {
        if (m_sensor != nullptr)
            ua_sensors_haptic_disable(m_sensor);

        if (m_tag)
            g_source_remove(m_tag);
    }

private:

    static gboolean on_timeout (gpointer gself)
    {
        static_cast<Impl*>(gself)->vibrate_now();
        return G_SOURCE_CONTINUE;
    }

    void vibrate_now()
    {
        const uint32_t msec = 1000;
        (void) ua_sensors_haptic_vibrate_once (m_sensor, msec);
    }

    const Mode m_mode;
    UASensorsHaptic * m_sensor = nullptr;
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
