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

#ifndef INDICATOR_DATETIME_STATE_H
#define INDICATOR_DATETIME_STATE_H

#include <datetime/clock.h>
#include <datetime/timezones.h>
#include <datetime/planner.h>
#include <datetime/locations.h>
   
#include <core/property.h>

#include <memory> // std::shared_ptr

namespace unity {
namespace indicator {
namespace datetime {

/**
 * \brief Aggregates all the classes that represent the backend state.
 *
 * This is where the app comes together. It's a model that aggregates
 * all of the backend appointments/alarms, locations, timezones,
 * system time, and so on. The "view" code (ie, the Menus) need to
 * respond to Signals from the State and update themselves accordingly.
 *
 * @see Menu
 * @see MenuFactory
 * @see Timezones
 * @see Clock
 * @see Planner
 * @see Locations
 */
struct State
{
    std::shared_ptr<Timezones> timezones;
    std::shared_ptr<Clock> clock;
    std::shared_ptr<Planner> planner;
    std::shared_ptr<Locations> locations;

    core::Property<bool> show_events;
    core::Property<bool> show_clock;
    core::Property<DateTime> calendar_day;
    core::Property<bool> show_week_numbers;
};

} // namespace datetime
} // namespace indicator
} // namespace unity

#endif // INDICATOR_DATETIME_STATE_H
