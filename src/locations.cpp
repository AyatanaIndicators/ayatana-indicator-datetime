/*
 * Copyright 2013 Canonical Ltd.
 * Copyright 2021 Robert Tari
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

#include <datetime/locations.h>

#include <glib.h>

namespace ayatana {
namespace indicator {
namespace datetime {

const std::string& Location::zone() const
{
    return m_zone;
}

const std::string& Location::name() const
{
    return m_name;
}

bool Location::operator== (const Location& that) const
{
    return (m_name == that.m_name)
        && (m_zone == that.m_zone)
        && (m_offset == that.m_offset);
}


Location::Location(const std::string& zone_, const std::string& name_):
    m_zone(zone_),
    m_name(name_)
{
    #if GLIB_CHECK_VERSION(2, 68, 0)
    auto gzone = g_time_zone_new_identifier(zone().c_str());

    if (gzone == NULL)
    {
        gzone = g_time_zone_new_utc();
    }
    #else
    auto gzone = g_time_zone_new (zone().c_str());
    #endif
    auto gtime = g_date_time_new_now (gzone);
    m_offset = g_date_time_get_utc_offset (gtime);
    g_date_time_unref (gtime);
    g_time_zone_unref (gzone);
}

} // namespace datetime
} // namespace indicator
} // namespace ayatana
