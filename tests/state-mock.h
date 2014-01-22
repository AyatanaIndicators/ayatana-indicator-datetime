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

#include <datetime/clock-mock.h>
#include <datetime/formatter.h>
#include <datetime/locations.h>
#include <datetime/menu.h>
#include <datetime/planner-mock.h>
#include <datetime/service.h>
#include <datetime/state.h>
#include <datetime/timezones.h>

using namespace unity::indicator::datetime;

class MockState: public State
{
public:
    std::shared_ptr<MockClock> mock_clock;

    MockState()
    {
        const DateTime now = DateTime::NowLocal();
        mock_clock.reset(new MockClock(now));
        settings.reset(new Settings);
        timezones.reset(new Timezones);
        clock = std::dynamic_pointer_cast<Clock>(mock_clock);
        planner.reset(new MockPlanner);
        planner->time = now;
        locations.reset(new Locations);
        calendar_day = now;
    }
};

