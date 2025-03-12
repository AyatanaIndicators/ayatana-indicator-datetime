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
    constexpr char const * zone_str {"Europe/Berlin"};
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
    const DateTime range_begin {gtz, 2015,7, 1, 0, 0, 0.0};
    const DateTime range_end   {gtz, 2015,7,31,23,59,59.5};
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
    std::array<Appointment,1> expected_appts1;
    auto appt1 = &expected_appts1[0];
#ifndef LOMIRI_FEATURES_ENABLED
    appt1->uid = "8ggc30kh89qql8vjumgtug7l14@google.com";
    appt1->color = "#becedd";
#else
    appt1->uid = "01fd35a6-8fbb-4c31-97e1-71d920190f18";
    appt1->color = "#0000FF";
#endif
    appt1->summary = "Hello";
    appt1->begin = DateTime{gtz,2015,7,1,20,0,0};
    appt1->end = DateTime{gtz,2015,7,1,22,0,0};

    std::array<Appointment,1> expected_appts2;
    auto appt2 = &expected_appts2[0];
#ifndef LOMIRI_FEATURES_ENABLED
    appt2->uid = "8ggc30kh89qql8vjumgtug7l14@google.com";
    appt2->color = "#62a0ea";
#else
    appt2->uid = "01fd35a6-8fbb-4c31-97e1-71d920190f18";
    appt2->color = "";
#endif
    appt2->summary = "Hello";
    appt2->begin = DateTime{gtz,2015,7,1,20,0,0};
    appt2->end = DateTime{gtz,2015,7,1,22,0,0};

    // compare it to what we actually loaded...
    const auto appts = planner->appointments().get();
    EXPECT_EQ(expected_appts1.size(), appts.size());
    EXPECT_EQ(expected_appts2.size(), appts.size());

    for (size_t i=0, n=std::min(appts.size(),expected_appts1.size()); i<n; i++)
    {
        EXPECT_PRED3([](auto pAppointmentIn, auto pAppointmentExpected1, auto pAppointmentExpected2)
        {
            return pAppointmentIn == pAppointmentExpected1 || pAppointmentIn == pAppointmentExpected2;
        }, appts[i], expected_appts1[i], expected_appts2[i]);
    }

    // cleanup
    g_time_zone_unref(gtz);
}
