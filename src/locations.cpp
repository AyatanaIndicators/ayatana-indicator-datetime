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

Location::Location(const std::string& zone_, const std::string& name_):
    zone(zone_),
    name(name_)
{
    GTimeZone * gzone = g_time_zone_new (zone.c_str());
    GDateTime * gtime = g_date_time_new_now (gzone);
    offset = g_date_time_get_utc_offset (gtime);
    g_date_time_unref (gtime);
    g_time_zone_unref (gzone);
}

} // namespace datetime
} // namespace indicator
} // namespace unity
