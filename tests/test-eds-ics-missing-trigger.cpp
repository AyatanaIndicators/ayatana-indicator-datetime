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

using namespace unity::indicator::datetime;
using VAlarmFixture = GlibFixture;

/***
****
***/

TEST_F(VAlarmFixture, MissingTriggers)
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

    // build expected: one-time alarm
    std::vector<Appointment> expected;
    Appointment a;
    a.type = Appointment::UBUNTU_ALARM;
    a.uid = "20150617T211838Z-6217-32011-2036-1@ubuntu-phablet";
    a.color = "#becedd";
    a.summary = "One Time Alarm";
    a.begin = DateTime { gtz, 2015, 6, 18, 10, 0, 0};
    a.end = a.begin;
    a.alarms.resize(1);
    a.alarms[0].audio_url = "file:///usr/share/sounds/ubuntu/ringtones/Alarm clock.ogg";
    a.alarms[0].time = a.begin;
    a.alarms[0].text = a.summary;
    expected.push_back(a);

    // build expected: recurring alarm
    a.uid = "20150617T211913Z-6217-32011-2036-5@ubuntu-phablet";
    a.summary = "Recurring Alarm";
    a.alarms[0].text = a.summary;
    std::array<DateTime,14> recurrences {
        DateTime{ gtz, 2015, 6, 18, 10, 1, 0 },
        DateTime{ gtz, 2015, 6, 19, 10, 1, 0 },
        DateTime{ gtz, 2015, 6, 20, 10, 1, 0 },
        DateTime{ gtz, 2015, 6, 21, 10, 1, 0 },
        DateTime{ gtz, 2015, 6, 22, 10, 1, 0 },
        DateTime{ gtz, 2015, 6, 23, 10, 1, 0 },
        DateTime{ gtz, 2015, 6, 24, 10, 1, 0 },
        DateTime{ gtz, 2015, 6, 25, 10, 1, 0 },
        DateTime{ gtz, 2015, 6, 26, 10, 1, 0 },
        DateTime{ gtz, 2015, 6, 27, 10, 1, 0 },
        DateTime{ gtz, 2015, 6, 28, 10, 1, 0 },
        DateTime{ gtz, 2015, 6, 29, 10, 1, 0 },
        DateTime{ gtz, 2015, 6, 30, 10, 1, 0 },
        DateTime{ gtz, 2015, 7,  1, 10, 1, 0 }
    };
    for (const auto& time : recurrences) {
        a.begin = a.end = a.alarms[0].time = time;
        expected.push_back(a);
    }

    // the planner should match what we've got in the calendar.ics file
    const auto appts = planner->appointments().get();
    EXPECT_EQ(expected, appts);

    // cleanup
    g_time_zone_unref(gtz);
}
