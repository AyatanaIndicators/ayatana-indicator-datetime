/*
 * Copyright 2015 Canonical Ltd.
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

#include <algorithm>

#include <datetime/alarm-queue-simple.h>
#include <datetime/clock-mock.h>
#include <datetime/engine-eds.h>
#include <datetime/planner-range.h>

#include <gtest/gtest.h>

#include "glib-fixture.h"
#include "print-to.h"
#include "timezone-mock.h"
#include "wakeup-timer-mock.h"

using namespace ayatana::indicator::datetime;
using VAlarmFixture = GlibFixture;

/***
****
***/

TEST_F(VAlarmFixture, MultipleAppointments)
{
    // start the EDS engine
    auto engine = std::make_shared<EdsEngine>();

    // we need a consistent timezone for the planner and our local DateTimes
    constexpr char const * zone_str {"America/Chicago"};
    auto tz = std::make_shared<MockTimezone>(zone_str);
    auto gtz = g_time_zone_new(zone_str);

    // make a planner that looks at the first half of 2015 in EDS
    auto planner = std::make_shared<SimpleRangePlanner>(engine, tz);
    const DateTime range_begin {gtz, 2015,1, 1, 0, 0, 0.0};
    const DateTime range_end   {gtz, 2015,6,31,23,59,59.5};
    planner->range().set(std::make_pair(range_begin, range_end));

    // give EDS a moment to load
    if (planner->appointments().get().empty()) {
        g_message("waiting a moment for EDS to load...");
        auto on_appointments_changed = [this](const std::vector<Appointment>& appointments){
            g_message("ah, they loaded");
            if (!appointments.empty())
                g_main_loop_quit(loop);
        };
        core::ScopedConnection conn(planner->appointments().changed().connect(on_appointments_changed));
        constexpr int max_wait_sec = 10;
        wait_msec(max_wait_sec * G_TIME_SPAN_MILLISECOND);
    }

    // the planner should match what we've got in the calendar.ics file
    const auto appts = planner->appointments().get();
    ASSERT_EQ(1, appts.size());
    const auto& appt = appts.front();
    ASSERT_EQ(8, appt.alarms.size());
    EXPECT_EQ(Alarm({"Time to pack!",      "", DateTime(gtz,2015,4,23,13,35,0)}), appt.alarms[0]);
    EXPECT_EQ(Alarm({"Time to pack!",      "", DateTime(gtz,2015,4,23,13,37,0)}), appt.alarms[1]);
    EXPECT_EQ(Alarm({"Time to pack!",      "", DateTime(gtz,2015,4,23,13,39,0)}), appt.alarms[2]);
    EXPECT_EQ(Alarm({"Time to pack!",      "", DateTime(gtz,2015,4,23,13,41,0)}), appt.alarms[3]);
    EXPECT_EQ(Alarm({"Go to the airport!", "", DateTime(gtz,2015,4,24,10,35,0)}), appt.alarms[4]);
    EXPECT_EQ(Alarm({"Go to the airport!", "", DateTime(gtz,2015,4,24,10,37,0)}), appt.alarms[5]);
    EXPECT_EQ(Alarm({"Go to the airport!", "", DateTime(gtz,2015,4,24,10,39,0)}), appt.alarms[6]);
    EXPECT_EQ(Alarm({"Go to the airport!", "", DateTime(gtz,2015,4,24,10,41,0)}), appt.alarms[7]);

    // now let's try this out with AlarmQueue...
    // hook the planner up to a SimpleAlarmQueue and confirm that it triggers for each of the reminders
    auto mock_clock = std::make_shared<MockClock>(range_begin);
    std::shared_ptr<Clock> clock = mock_clock;
    std::shared_ptr<WakeupTimer> wakeup_timer = std::make_shared<MockWakeupTimer>(clock);
    auto alarm_queue = std::make_shared<SimpleAlarmQueue>(clock, planner, wakeup_timer);
    int triggered_count = 0;
    alarm_queue->alarm_reached().connect([&triggered_count, appt](const Appointment&, const Alarm& active_alarm) {
        EXPECT_TRUE(std::find(appt.alarms.begin(), appt.alarms.end(), active_alarm) != appt.alarms.end());
        ++triggered_count;
    });
    for (auto now=range_begin; now<range_end; now+=std::chrono::minutes{1})
      mock_clock->set_localtime(now);
    EXPECT_EQ(appt.alarms.size(), triggered_count);

    // cleanup
    g_time_zone_unref(gtz);
}
