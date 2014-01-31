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

#include <datetime/actions.h>

#include "state-fixture.h"

using namespace unity::indicator::datetime;

typedef StateFixture ActionsFixture;

TEST_F(ActionsFixture, ActionsExist)
{
    EXPECT_TRUE(m_actions != nullptr);

    const char* names[] = { "desktop-header",
                            "calendar",
                            "set-location",
                            "activate-planner",
                            "activate-appointment",
                            "activate-phone-settings",
                            "activate-phone-clock-app",
                            "activate-desktop-settings" };
    for(const auto& name: names)
    {
        EXPECT_TRUE(g_action_group_has_action(m_actions->action_group(), name));
    }
}

TEST_F(ActionsFixture, ActivateDesktopSettings)
{
    const auto action_name = "activate-desktop-settings";
    const auto expected_action = MockActions::OpenDesktopSettings;

    auto action_group = m_actions->action_group();
    auto history = m_mock_actions->history();
    EXPECT_EQ(0, history.size());
    EXPECT_TRUE(g_action_group_has_action(action_group, action_name));

    g_action_group_activate_action(action_group, action_name, nullptr);
    history = m_mock_actions->history();
    EXPECT_EQ(1, history.size());
    EXPECT_EQ(expected_action, history[0]);
}

TEST_F(ActionsFixture, ActivatePhoneSettings)
{
    const auto action_name = "activate-phone-settings";
    const auto expected_action = MockActions::OpenPhoneSettings;

    auto action_group = m_actions->action_group();
    EXPECT_TRUE(m_mock_actions->history().empty());
    EXPECT_TRUE(g_action_group_has_action(action_group, action_name));

    g_action_group_activate_action(action_group, action_name, nullptr);
    auto history = m_mock_actions->history();
    EXPECT_EQ(1, history.size());
    EXPECT_EQ(expected_action, history[0]);
}

TEST_F(ActionsFixture, ActivatePhoneClockApp)
{
    const auto action_name = "activate-phone-clock-app";
    const auto expected_action = MockActions::OpenPhoneClockApp;

    auto action_group = m_actions->action_group();
    EXPECT_TRUE(m_mock_actions->history().empty());
    EXPECT_TRUE(g_action_group_has_action(action_group, action_name));

    g_action_group_activate_action(action_group, action_name, nullptr);
    auto history = m_mock_actions->history();
    EXPECT_EQ(1, history.size());
    EXPECT_EQ(expected_action, history[0]);
}

TEST_F(ActionsFixture, ActivatePlanner)
{
    const auto action_name = "activate-planner";
    auto action_group = m_actions->action_group();
    EXPECT_TRUE(m_mock_actions->history().empty());
    EXPECT_TRUE(g_action_group_has_action(action_group, action_name));

    const auto expected_action = MockActions::OpenPlanner;
    auto v = g_variant_new_int64(0);
    g_action_group_activate_action(action_group, action_name, v);
    auto history = m_mock_actions->history();
    EXPECT_EQ(1, history.size());
    EXPECT_EQ(expected_action, history[0]);
}

TEST_F(ActionsFixture, ActivatePlannerAt)
{
    const auto action_name = "activate-planner";
    auto action_group = m_actions->action_group();
    EXPECT_TRUE(m_mock_actions->history().empty());
    EXPECT_TRUE(g_action_group_has_action(action_group, action_name));

    const auto now = DateTime::NowLocal();
    auto v = g_variant_new_int64(now.to_unix());
    g_action_group_activate_action(action_group, action_name, v);
    const auto a = MockActions::OpenPlannerAt;
    EXPECT_EQ(std::vector<MockActions::Action>({a}), m_mock_actions->history());
    EXPECT_EQ(now.to_unix(), m_mock_actions->date_time().to_unix());
}

TEST_F(ActionsFixture, SetLocation)
{
    const auto action_name = "set-location";
    auto action_group = m_actions->action_group();
    EXPECT_TRUE(m_mock_actions->history().empty());
    EXPECT_TRUE(g_action_group_has_action(action_group, action_name));

    auto v = g_variant_new_string("America/Chicago Oklahoma City");
    g_action_group_activate_action(action_group, action_name, v);
    const auto expected_action = MockActions::SetLocation;
    ASSERT_EQ(1, m_mock_actions->history().size());
    EXPECT_EQ(expected_action, m_mock_actions->history()[0]);
    EXPECT_EQ("America/Chicago", m_mock_actions->zone());
    EXPECT_EQ("Oklahoma City", m_mock_actions->name());
}

TEST_F(ActionsFixture, SetCalendarDate)
{
    // confirm that such an action exists
    const auto action_name = "calendar";
    auto action_group = m_actions->action_group();
    EXPECT_TRUE(m_mock_actions->history().empty());
    EXPECT_TRUE(g_action_group_has_action(action_group, action_name));

    // pick an arbitrary DateTime...
    auto tmp = g_date_time_new_local(2010, 1, 2, 3, 4, 5);
    const auto now = DateTime(tmp);
    g_date_time_unref(tmp);

    // confirm that Planner.time gets changed to that date when we
    // activate the 'calendar' action with that date's time_t as the arg
    EXPECT_NE (now, m_state->planner->time.get());
    auto v = g_variant_new_int64(now.to_unix());
    g_action_group_activate_action (action_group, action_name, v);
    EXPECT_EQ (now, m_state->planner->time.get());
}

TEST_F(ActionsFixture, ActivatingTheCalendarResetsItsDate)
{
    // Confirm that the GActions exist
    auto action_group = m_actions->action_group();
    EXPECT_TRUE(g_action_group_has_action(action_group, "calendar"));
    EXPECT_TRUE(g_action_group_has_action(action_group, "calendar-active"));

    ///
    /// Prerequisite for the test: move calendar-date away from today
    ///

    // move calendar-date a week into the future...
    const auto now = m_state->clock->localtime();
    auto next_week = g_date_time_add_weeks(now.get(), 1);
    const auto next_week_unix = g_date_time_to_unix(next_week);
    g_action_group_activate_action (action_group, "calendar", g_variant_new_int64(next_week_unix));

    // confirm the planner and calendar action state moved a week into the future
    // but that m_state->clock is unchanged
    EXPECT_EQ(next_week_unix, m_state->planner->time.get().to_unix());
    EXPECT_EQ(now, m_state->clock->localtime());
    auto calendar_state = g_action_group_get_action_state(action_group, "calendar");
    EXPECT_TRUE(calendar_state != nullptr);
    EXPECT_TRUE(g_variant_is_of_type(calendar_state, G_VARIANT_TYPE_DICTIONARY));
    auto v = g_variant_lookup_value(calendar_state, "calendar-day", G_VARIANT_TYPE_INT64);
    EXPECT_TRUE(v != nullptr);
    EXPECT_EQ(next_week_unix, g_variant_get_int64(v));
    g_clear_pointer(&v, g_variant_unref);
    g_clear_pointer(&calendar_state, g_variant_unref);

    ///
    /// Now the actual test.
    /// We set the state of 'calendar-active' to true, which should reset the calendar date.
    /// This is so the calendar always starts on today's date when the indicator's menu is pulled down.
    ///

    // change the state...
    g_action_group_change_action_state(action_group, "calendar-active", g_variant_new_boolean(true));

    // confirm the planner and calendar action state were reset back to m_state->clock's time
    EXPECT_EQ(now.to_unix(), m_state->planner->time.get().to_unix());
    EXPECT_EQ(now, m_state->clock->localtime());
    calendar_state = g_action_group_get_action_state(action_group, "calendar");
    EXPECT_TRUE(calendar_state != nullptr);
    EXPECT_TRUE(g_variant_is_of_type(calendar_state, G_VARIANT_TYPE_DICTIONARY));
    v = g_variant_lookup_value(calendar_state, "calendar-day", G_VARIANT_TYPE_INT64);
    EXPECT_TRUE(v != nullptr);
    EXPECT_EQ(now.to_unix(), g_variant_get_int64(v));
    g_clear_pointer(&v, g_variant_unref);
    g_clear_pointer(&calendar_state, g_variant_unref);

}


TEST_F(ActionsFixture, OpenAppointment)
{
    Appointment appt;
    appt.uid = "some arbitrary uid";
    appt.url = "http://www.canonical.com/";
    m_state->planner->upcoming.set(std::vector<Appointment>({appt}));

    const auto action_name = "activate-appointment";
    auto action_group = m_actions->action_group();
    EXPECT_TRUE(m_mock_actions->history().empty());
    EXPECT_TRUE(g_action_group_has_action(action_group, action_name));

    auto v = g_variant_new_string(appt.uid.c_str());
    g_action_group_activate_action(action_group, action_name, v);
    const auto a = MockActions::OpenAppointment;
    ASSERT_EQ(1, m_mock_actions->history().size());
    ASSERT_EQ(a, m_mock_actions->history()[0]);
    EXPECT_EQ(appt.url, m_mock_actions->url());
}

