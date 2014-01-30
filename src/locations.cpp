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

#include <datetime/locations.h>

#include <glib.h>

namespace unity {
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

Location::Location(const std::string& zone_, const std::string& name_):
    m_zone(zone_),
    m_name(name_)
{
    auto gzone = g_time_zone_new (zone().c_str());
    auto gtime = g_date_time_new_now (gzone);
    m_offset = g_date_time_get_utc_offset (gtime);
    g_date_time_unref (gtime);
    g_time_zone_unref (gzone);
}

#if 0
DateTime Location::localtime(const DateTime& reference_point) const
{
GDateTime *         g_date_time_to_timezone             (GDateTime *datetime,
                                                         GTimeZone *tz);
    auto gzone = g_time_zone_new(zone().c_str());
    const auto gtime = reference_point.get();
    auto glocal = g_date_time_new (gzone,
                                   g_date_time_get_year(gtime),
                                   g_date_time_get_month(gtime),
                                   g_date_time_get_day_of_month(gtime),
                                   g_date_time_get_hour(gtime),
                                   g_date_time_get_minute(gtime),
                                   g_date_time_get_seconds(gtime));
    DateTime local(glocal);
    g_date_time_unref(glocal);
    g_message("reference: %zu", (size_t)reference_point.to_unix(), (size_t)local.to_unix());
    //g_date_time_unref(gtime);
    g_time_zone_unref(gzone);
    return local;
}
#endif

} // namespace datetime
} // namespace indicator
} // namespace unity
