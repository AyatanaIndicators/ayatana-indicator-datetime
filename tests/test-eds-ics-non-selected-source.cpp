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
 *   Renato Araujo Oliveira Filh <renato.filho@canonical.com>
 */

#include <algorithm>

#include <datetime/engine-eds.h>
#include <datetime/myself.h>
#include <datetime/planner-range.h>

#include <libedataserver/libedataserver.h>

#include <gtest/gtest.h>

#include "glib-fixture.h"
#include "timezone-mock.h"

using namespace ayatana::indicator::datetime;
using VAlarmFixture = GlibFixture;

/***
****
***/

TEST_F(VAlarmFixture, NonSelectedSources)
{
    // start the EDS engine
    auto engine = std::make_shared<EdsEngine>(std::make_shared<Myself>());

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

    // appointmes are visible
    auto appts = planner->appointments().get();
    EXPECT_TRUE(appts.size() > 0);

    // Unselect all sources
    auto registry = e_source_registry_new_sync(NULL, NULL);
    auto sources = e_source_registry_list_sources(registry, E_SOURCE_EXTENSION_TASK_LIST);
    for (auto l=sources; l!=nullptr; l=l->next) {
        auto source =  static_cast<ESource*>(l->data);
        auto extension = e_source_get_extension(source, E_SOURCE_EXTENSION_CALENDAR);
        e_source_selectable_set_selected(E_SOURCE_SELECTABLE(extension), FALSE);
        e_source_write_sync(source, NULL, NULL);
    }

    g_list_free_full(sources, g_object_unref);
    g_object_unref(registry);

    // give some time to planner update
    wait_msec(5 * G_TIME_SPAN_MILLISECOND);

    // the planner should be empty at this point
    appts = planner->appointments().get();
    EXPECT_TRUE(appts.size() == 0);
    // cleanup
    g_time_zone_unref(gtz);
}
