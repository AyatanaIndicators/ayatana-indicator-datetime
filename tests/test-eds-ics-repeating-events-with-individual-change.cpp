/*
 * Copyright 2016 Canonical Ltd.
 * Copyright 2021-2022 Robert Tari
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
 *   Renato Araujo Oliveira Filho <renato.filho@canonical.com>
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

TEST_F(VAlarmFixture, RepeatingEventsWithIndividualChange)
{
    // start the EDS engine
    auto engine = std::make_shared<EdsEngine>(std::make_shared<Myself>());

    // we need a consistent timezone for the planner and our local DateTimes
    constexpr char const * zone_str {"America/Recife"};
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

    // make a planner that looks at the year of 2016 in EDS
    auto planner = std::make_shared<SimpleRangePlanner>(engine, tz);
    const DateTime range_begin {gtz, 2016,1, 1, 0, 0, 0.0};
    const DateTime range_end   {gtz, 2016,12,31,23,59,59.5};
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
    expected_appt.summary = "Alarm";
    std::array<DateTime,10> expected_times = {
        DateTime(gtz,2016,6, 20,10,00,0),
        DateTime(gtz,2016,6, 21,10,00,0),
        DateTime(gtz,2016,6, 22,10,00,0),
        DateTime(gtz,2016,6, 23,10,00,0),
        DateTime(gtz,2016,6, 24,20,00,0),
        DateTime(gtz,2016,6, 25,10,00,0),
        DateTime(gtz,2016,6, 26,10,00,0),
        DateTime(gtz,2016,6, 27,10,00,0),
        DateTime(gtz,2016,6, 28,10,00,0),
        DateTime(gtz,2016,6, 29,10,00,0)
    };

    // compare it to what we actually loaded...
    const auto appts = planner->appointments().get();
    EXPECT_EQ(expected_times.size(), appts.size());
    for (size_t i=0, n=expected_times.size(); i<n; i++) {
        const auto& appt = appts[i];
        if (i != 4)
            EXPECT_EQ("Every day and every night", appt.summary);
        else
            EXPECT_EQ("At night", appt.summary);
        EXPECT_EQ(expected_times[i], appt.begin);
    }

    // cleanup
    g_time_zone_unref(gtz);
}
