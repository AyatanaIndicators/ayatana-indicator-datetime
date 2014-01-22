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

#include <datetime/state-live.h>

#include <datetime/clock.h>
#include <datetime/locations-settings.h>
#include <datetime/planner-eds.h>
#include <datetime/settings-live.h>
#include <datetime/state.h>
#include <datetime/timezones-live.h>

namespace unity {
namespace indicator {
namespace datetime {

/***
****
***/

LiveState::LiveState()
{
    std::shared_ptr<Settings> live_settings(new LiveSettings);
    std::shared_ptr<Timezones> live_timezones(new LiveTimezones(live_settings, TIMEZONE_FILE));
    std::shared_ptr<Clock> live_clock(new LiveClock(live_timezones));

    settings = live_settings;
    clock = live_clock;
    locations.reset(new SettingsLocations(live_settings, live_timezones));
    planner.reset(new PlannerEds);
    planner->time = clock->localtime();
    calendar_day = clock->localtime();
}

/***
****
***/

} // namespace datetime
} // namespace indicator
} // namespace unity
