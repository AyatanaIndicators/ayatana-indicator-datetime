/*
 * Copyright 2015 Canonical Ltd.
 * Copyright 2021-2025 Robert Tari
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
 *   Robert Tari <robert@tari.in>
 */

#include <algorithm>
#include <array>
#include <datetime/alarm-queue-simple.h>
#include <datetime/clock-mock.h>
#include <datetime/engine-eds.h>
#include <datetime/myself.h>
#include <datetime/planner-range.h>

#include <gtest/gtest.h>

#include "glib-fixture.h"
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
    auto engine = std::make_shared<EdsEngine>(std::make_shared<Myself>());

    // we need a consistent timezone for the planner and our local DateTimes
    constexpr char const * zone_str {"America/Chicago"};
    auto tz = std::make_shared<MockTimezone>(zone_str);
    #if GLIB_CHECK_VERSION(2, 68, 0)
    auto gtz = g_time_zone_new_identifier(zone_str);

    if (gtz == NULL)
    {
        gtz = g_time_zone_new_utc();
    }
    #else
    auto gtz = g_time_zone_new(zone_str);
    #endif

    // make a planner that looks at the first half of 2015 in EDS
    auto planner = std::make_shared<SimpleRangePlanner>(engine, tz);
    const DateTime range_begin {gtz, 2015,1, 1, 0, 0, 0.0};
    const DateTime range_end   {gtz, 2015,6,30,23,59,59.5};
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

    // what we expect to get...
    Appointment expected_appt;
#ifndef LOMIRI_FEATURES_ENABLED
    expected_appt.uid = "20150520T000726Z-3878-32011-1770-81@lomiri-phablet";
#else
    expected_appt.uid = "d7aeb192-8b2c-4427-834f-f30388e9e73c";
#endif
    expected_appt.summary = "Alarm";
    std::array<Alarm,1> expected_alarms = {
        Alarm({"Alarm", "file://" ALARM_DEFAULT_SOUND, DateTime(gtz,2015,5,20,20,00,0)})
    };

    // compare it to what we actually loaded...
    const auto appts = planner->appointments().get();
    EXPECT_EQ(expected_alarms.size(), appts.size());
    for (size_t i=0, n=expected_alarms.size(); i<n; i++) {
        const auto& appt = appts[i];
        EXPECT_EQ(expected_appt.uid, appt.uid);
        EXPECT_EQ(expected_appt.summary, appt.summary);
        EXPECT_EQ(1, appt.alarms.size());
        EXPECT_EQ(expected_alarms[i], appt.alarms[0]);

        EXPECT_PRED3([](auto sColourIn, auto sColourExpected1, auto sColourExpected2)
        {
            return sColourIn == sColourExpected1 || sColourIn == sColourExpected2;
    #ifndef LOMIRI_FEATURES_ENABLED
        }, appt.color, "#becedd", "#62a0ea");
    #else
        }, appt.color, "#0000FF", "");
    #endif
    }

    // cleanup
    g_time_zone_unref(gtz);
}
