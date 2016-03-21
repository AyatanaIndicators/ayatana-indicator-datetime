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

#include "timedated-fixture.h"

/***
****
***/

TEST_F(TimedateFixture, HelloWorld)
{
    EXPECT_TRUE(true);
}

TEST_F(TimedateFixture, SetLocation)
{
    const std::string tzid = "America/Chicago";
    const std::string name = "Oklahoma City";
    const std::string expected = tzid + " " + name;

    EXPECT_NE(expected, m_state->settings->timezone_name.get());

    m_actions->set_location(tzid, name);
    m_state->settings->timezone_name.changed().connect(
          [this](const std::string&){
            g_main_loop_quit(loop);
            });
    g_main_loop_run(loop);
    EXPECT_EQ(attempted_tzid, tzid);
    wait_msec();

    EXPECT_EQ(expected, m_state->settings->timezone_name.get());
}

/***
****
***/

TEST_F(TimedateFixture, DesktopOpenAlarmApp)
{
    m_actions->desktop_open_alarm_app();
    const std::string expected = "evolution -c calendar";
    EXPECT_EQ(expected, m_live_actions->last_cmd);
}

TEST_F(TimedateFixture, DesktopOpenAppointment)
{
    Appointment a;
    a.uid = "some-uid";
    a.begin = DateTime::NowLocal();
    m_actions->desktop_open_appointment(a, a.begin);
    const std::string expected_substr = "evolution \"calendar:///?startdate=";
    EXPECT_NE(m_live_actions->last_cmd.find(expected_substr), std::string::npos);
}

TEST_F(TimedateFixture, DesktopOpenCalendarApp)
{
    m_actions->desktop_open_calendar_app(DateTime::NowLocal());
    const std::string expected_substr = "evolution \"calendar:///?startdate=";
    EXPECT_NE(m_live_actions->last_cmd.find(expected_substr), std::string::npos);
}

TEST_F(TimedateFixture, DesktopOpenSettingsApp)
{
    m_actions->desktop_open_settings_app();
    const std::string expected_substr = "control-center";
    EXPECT_NE(m_live_actions->last_cmd.find(expected_substr), std::string::npos);
}

/***
****
***/

namespace
{
    const std::string clock_app_url = "appid://com.ubuntu.clock/clock/current-user-version";
}

TEST_F(TimedateFixture, PhoneOpenAlarmApp)
{
    m_actions->phone_open_alarm_app();
    EXPECT_EQ(clock_app_url, m_live_actions->last_url);
}

TEST_F(TimedateFixture, PhoneOpenAppointment)
{
    Appointment a;

    a.uid = "event-uid";
    a.source_uid = "source-uid";
    a.begin = DateTime::NowLocal();
    a.type = Appointment::EVENT;
    auto ocurrenceDate = DateTime::Local(2014, 1, 1, 0, 0, 0);
    m_actions->phone_open_appointment(a, ocurrenceDate);
    const std::string appointment_app_url =  ocurrenceDate.to_timezone("UTC").format("calendar://startdate=%Y-%m-%dT%H:%M:%S+00:00");
    EXPECT_EQ(appointment_app_url, m_live_actions->last_url);

    a.type = Appointment::UBUNTU_ALARM;
    m_actions->phone_open_appointment(a, a.begin);
    EXPECT_EQ(clock_app_url, m_live_actions->last_url);
}

TEST_F(TimedateFixture, PhoneOpenCalendarApp)
{
    auto now = DateTime::NowLocal();
    m_actions->phone_open_calendar_app(now);
    const std::string expected =  now.to_timezone("UTC").format("calendar://startdate=%Y-%m-%dT%H:%M:%S+00:00");
    EXPECT_EQ(expected, m_live_actions->last_url);
}


TEST_F(TimedateFixture, PhoneOpenSettingsApp)
{
    m_actions->phone_open_settings_app();
    const std::string expected = "settings:///system/time-date";
    EXPECT_EQ(expected, m_live_actions->last_url);
}

/***
****
***/

TEST_F(TimedateFixture, CalendarState)
{
    // init the clock
    auto now = DateTime::Local(2014, 1, 1, 0, 0, 0);
    m_mock_state->mock_clock->set_localtime (now);
    m_state->calendar_month->month().set(now);
    //m_state->planner->time.set(now);

    ///
    ///  Test the default calendar state.
    ///

    auto action_group = m_actions->action_group();
    auto calendar_state = g_action_group_get_action_state (action_group, "calendar");
    EXPECT_TRUE (calendar_state != nullptr);
    EXPECT_TRUE (g_variant_is_of_type (calendar_state, G_VARIANT_TYPE_DICTIONARY));

    // there's nothing in the planner yet, so appointment-days should be an empty array
    auto v = g_variant_lookup_value (calendar_state, "appointment-days", G_VARIANT_TYPE_ARRAY);
    EXPECT_TRUE (v != nullptr);
    EXPECT_EQ (0, g_variant_n_children (v));
    g_clear_pointer (&v, g_variant_unref);

    // calendar-day should be in sync with m_state->calendar_day
    v = g_variant_lookup_value (calendar_state, "calendar-day", G_VARIANT_TYPE_INT64);
    EXPECT_TRUE (v != nullptr);
    EXPECT_EQ (m_state->calendar_month->month().get().to_unix(), g_variant_get_int64(v));
    g_clear_pointer (&v, g_variant_unref);

    // show-week-numbers should be false because MockSettings defaults everything to 0
    v = g_variant_lookup_value (calendar_state, "show-week-numbers", G_VARIANT_TYPE_BOOLEAN);
    EXPECT_TRUE (v != nullptr);
    EXPECT_FALSE (g_variant_get_boolean (v));
    g_clear_pointer (&v, g_variant_unref);

    // cleanup this step
    g_clear_pointer (&calendar_state, g_variant_unref);


    ///
    ///  Now add appointments to the planner and confirm that the state keeps in sync
    ///

    auto tomorrow = now.add_days(1);
    auto tomorrow_begin = tomorrow.start_of_day();
    auto tomorrow_end = tomorrow.end_of_day();
    Appointment a1;
    a1.color = "green";
    a1.summary = "write unit tests";
    a1.uid = "D4B57D50247291478ED31DED17FF0A9838DED402";
    a1.begin = tomorrow_begin;
    a1.end = tomorrow_end;

    auto ubermorgen = now.add_days(2);
    auto ubermorgen_begin = ubermorgen.start_of_day();
    auto ubermorgen_end = ubermorgen.end_of_day();
    Appointment a2;
    a2.color = "orange";
    a2.summary = "code review";
    a2.uid = "2756ff7de3745bbffd65d2e4779c37c7ca60d843";
    a2.begin = ubermorgen_begin;
    a2.end = ubermorgen_end;

    m_state->calendar_month->appointments().set(std::vector<Appointment>({a1, a2}));

    ///
    ///  Now test the calendar state again.
    ///  The this_month field should now contain the appointments we just added.
    ///

    calendar_state = g_action_group_get_action_state (action_group, "calendar");
    v = g_variant_lookup_value (calendar_state, "appointment-days", G_VARIANT_TYPE_ARRAY);
    EXPECT_TRUE (v != nullptr);
    int i;
    g_variant_get_child (v, 0, "i", &i);
    EXPECT_EQ (a1.begin.day_of_month(), i);
    g_variant_get_child (v, 1, "i", &i);
    EXPECT_EQ (a2.begin.day_of_month(), i);
    g_clear_pointer(&v, g_variant_unref);
    g_clear_pointer(&calendar_state, g_variant_unref);

    ///
    ///  Confirm that the action state's dictionary
    ///  keeps in sync with settings.show_week_numbers
    ///

    auto b = m_state->settings->show_week_numbers.get();
    for (i=0; i<2; i++)
    {
        b = !b;
        m_state->settings->show_week_numbers.set(b);

        calendar_state = g_action_group_get_action_state (action_group, "calendar");
        v = g_variant_lookup_value (calendar_state, "show-week-numbers", G_VARIANT_TYPE_BOOLEAN);
        EXPECT_TRUE(v != nullptr);
        EXPECT_EQ(b, g_variant_get_boolean(v));

        g_clear_pointer(&v, g_variant_unref);
        g_clear_pointer(&calendar_state, g_variant_unref);
    }
}
