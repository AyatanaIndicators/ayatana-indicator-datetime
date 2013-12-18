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

#ifndef INDICATOR_DATETIME_LOCATIONS_H
#define INDICATOR_DATETIME_LOCATIONS_H

#include <core/property.h>

#include <string>
#include <vector>

namespace unity {
namespace indicator {
namespace datetime {

/**
 * \brief A physical place and its timezone; eg, "America/Chicago" + "Oklahoma City"
 */
struct Location
{
    /** timezone; eg, "America/Chicago" */
    std::string zone;

    /* human-readable location name; eg, "Oklahoma City" */
    std::string name;

    /** offset from UTC in microseconds */
    int64_t offset = 0;

    bool operator== (const Location& that) const
    {
        return (name == that.name) && (zone == that.zone) && (offset == that.offset);
    }

    Location (const std::string& zone, const std::string& name);
};

/**
 * A container for an ordered list of Locations.
 */
class Locations
{
public:
    Locations() =default;
    virtual ~Locations() =default;

    /** \brief an ordered list of Location items */
    core::Property<std::vector<Location> > locations;
};

} // namespace datetime
} // namespace indicator
} // namespace unity

#endif // INDICATOR_DATETIME_LOCATIONS_H
