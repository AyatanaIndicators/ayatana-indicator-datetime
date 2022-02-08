/*
 * Copyright 2016 Canonical Ltd.
 * Copyright 2021 Robert Tari
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

#include "glib-fixture.h"

#include <datetime/appointment.h>
#include <datetime/menu.h>
#include <algorithm>
#include <vector>

using MenuAppointmentFixture = GlibFixture;

using namespace ayatana::indicator::datetime;

namespace
{
    Appointment create_appointment(
        const Appointment::Type& type,
        const std::string& uid,
        const std::string& summary,
        const DateTime& begin,
        const DateTime& end)
    {
        Appointment a;
        a.type = type;
        a.uid = uid;
        a.summary = summary;
        a.begin = begin;
        a.end = end;
        return a;
    }
}

TEST_F(MenuAppointmentFixture, DisplayEvents)
{
    const auto airport = create_appointment(
        Appointment::ALARM,
        "uid-airport",
        "Pick Aunt Mabel up at the airport",
        DateTime::Local(2016,12,24,10,0,0),
        DateTime::Local(2016,12,24,10,0,0)
    );

    const auto christmas_eve_candle_service = create_appointment(
        Appointment::EVENT,
        "uid-christmas-eve-candle-service",
        "Christmas Eve Candle Service",
        DateTime::Local(2016,12,24,22,0,0),
        DateTime::Local(2016,12,24,23,0,0)
    );

    const auto christmas = create_appointment(
        Appointment::EVENT,
        "uid-christmas",
        "Christmas",
        DateTime::Local(2016,12,25,0,0,0),
        DateTime::Local(2016,12,26,0,0,0)
    );

    const auto santa = create_appointment(
        Appointment::ALARM,
        "uid-santa",
        "Time to set out cookies and milk for Santa",
        DateTime::Local(2016,12,25,1,0,0),
        DateTime::Local(2016,12,25,1,0,0)
    );

    const auto bike = create_appointment(
        Appointment::ALARM,
        "uid-bike",
        "Remember to put out the bike, it's in the garage",
        DateTime::Local(2016,12,25,1,0,0),
        DateTime::Local(2016,12,25,1,0,0)
    );


    const auto christmas_lunch = create_appointment(
        Appointment::EVENT,
        "uid-christmas-lunch",
        "Christmas Lunch at Grandma's",
        DateTime::Local(2016,12,25,12,0,0),
        DateTime::Local(2016,12,25,14,0,0)
    );

    const auto boxing_day = create_appointment(
        Appointment::EVENT,
        "uid-boxing-day",
        "Boxing Day",
        DateTime::Local(2016,12,26,0,0,0),
        DateTime::Local(2016,12,27,0,0,0)
    );

    const auto new_years_eve = create_appointment(
        Appointment::EVENT,
        "uid-new-years-eve",
        "New Years' Eve",
        DateTime::Local(2016,12,31,0,0,0),
        DateTime::Local(2017,1,1,0,0,0)
    );

    const struct
    {
        const char* const description;
        DateTime start;
        std::vector<Appointment> appointments;
        std::vector<Appointment> expected_display_appointments;
        int max_items;
    }
    tests[] =
    {
        {
            "test presentation order: full-day events come before part-day events",
            DateTime::Local(2016,12,25,6,0,0),
            std::vector<Appointment>({christmas, christmas_lunch, boxing_day, new_years_eve}),
            std::vector<Appointment>({christmas, christmas_lunch, boxing_day, new_years_eve}),
            5
        },
        {
            "test presentation order: part-day events in chronological order",
            DateTime::Local(2016,12,24,0,0,0),
            std::vector<Appointment>({airport, christmas_eve_candle_service, christmas, santa, christmas_lunch}),
            std::vector<Appointment>({airport, christmas_eve_candle_service, christmas, santa, christmas_lunch}),
            5
        },
        {
            "test presentation order: multiple events with the same start+end sorted alphabetically",
            DateTime::Local(2016,12,25,0,0,0),
            std::vector<Appointment>({christmas, bike, santa, christmas_lunch}),
            std::vector<Appointment>({christmas, bike, santa, christmas_lunch}),
            5
        },
        {
            "test culling priority: today's part-day events outrank today's full-day events",
            DateTime::Local(2016,12,25,1,0,0),
            std::vector<Appointment>({christmas, santa, christmas_lunch}),
            std::vector<Appointment>({santa, christmas_lunch}),
            2
        },
        {
            "test culling priority: events later today outrank both tomorrow's full-day and part-day events",
            DateTime::Local(2016,12,24,0,0,0),
            std::vector<Appointment>({christmas_eve_candle_service, christmas, santa}),
            std::vector<Appointment>({christmas_eve_candle_service}),
            1
        },
        {
            "test edge cases: confirm <max events works ok",
            DateTime::Local(2016,12,24,0,0,0),
            std::vector<Appointment>({christmas, christmas_lunch}),
            std::vector<Appointment>({christmas, christmas_lunch}),
            5
        },
        {
            "test edge cases: confirm 0 events works ok",
            DateTime::Local(2016,12,24,0,0,0),
            std::vector<Appointment>({}),
            std::vector<Appointment>({}),
            5
        },
        {
            "test edge cases: confirm max-events of 0 doesn't crash",
            DateTime::Local(2016,12,24,0,0,0),
            std::vector<Appointment>({christmas, bike, santa, christmas_lunch}),
            std::vector<Appointment>({}),
            0
        }
    };

    for (const auto& test : tests)
    {
        g_debug("running test: %s", test.description);

        // run the test...
        ASSERT_EQ(test.expected_display_appointments, Menu::get_display_appointments(test.appointments, test.start, test.max_items));

        // ...and again with a reversed vector to confirm input order doesn't matter
        auto reversed = test.appointments;
        std::reverse(reversed.begin(), reversed.end());
        ASSERT_EQ(test.expected_display_appointments, Menu::get_display_appointments(reversed, test.start, test.max_items));
    }
}

