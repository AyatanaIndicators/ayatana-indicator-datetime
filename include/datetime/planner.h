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

#ifndef INDICATOR_DATETIME_PLANNER_H
#define INDICATOR_DATETIME_PLANNER_H

#include <datetime/appointment.h>
#include <datetime/date-time.h>

#include <core/property.h>

#include <vector>

namespace ayatana {
namespace indicator {
namespace datetime {

/**
 * \brief Simple collection of appointments
 */
class Planner
{
public:
    virtual ~Planner();
    virtual core::Property<std::vector<Appointment>>& appointments() =0;

protected:
    Planner();
    static void sort(std::vector<Appointment>&);
};

} // namespace datetime
} // namespace indicator
} // namespace ayatana

#endif // INDICATOR_DATETIME_PLANNER_H
