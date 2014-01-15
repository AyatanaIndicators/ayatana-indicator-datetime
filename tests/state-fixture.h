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

#include "glib-fixture.h"
#include "actions-mock.h"

#include <datetime/clock-mock.h>
#include <datetime/formatter.h>
#include <datetime/locations.h>
#include <datetime/menu.h>
#include <datetime/planner-mock.h>
#include <datetime/service.h>
#include <datetime/state.h>
#include <datetime/timezones.h>

using namespace unity::indicator::datetime;

class StateFixture: public GlibFixture
{
private:
    typedef GlibFixture super;

protected:
    std::shared_ptr<MockClock> m_clock;
    std::shared_ptr<State> m_state;
    std::shared_ptr<MockActions> m_mock_actions;
    std::shared_ptr<Actions> m_actions;

    virtual void SetUp()
    {
        super::SetUp();

        // first, build a mock backend state
        const DateTime now = DateTime::NowLocal();
        m_clock.reset(new MockClock(now));
        m_state.reset(new State);
        m_state->timezones.reset(new Timezones);
        m_state->clock = std::dynamic_pointer_cast<Clock>(m_clock);
        m_state->planner.reset(new MockPlanner);
        m_state->planner->time = now;
        m_state->locations.reset(new Locations);
        m_state->calendar_day = now;

        // build the actions on top of the state
        m_mock_actions.reset(new MockActions(m_state));
        m_actions = std::dynamic_pointer_cast<Actions>(m_mock_actions);
    }

    virtual void TearDown()
    {
        m_actions.reset();
        m_mock_actions.reset();
        m_state.reset();
        m_clock.reset();

        super::TearDown();
    }
};

