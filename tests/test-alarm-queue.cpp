/*
 * Copyright 2014 Canonical Ltd.
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

#include <datetime/alarm-queue-simple.h>
#include <datetime/wakeup-timer-mainloop.h>

#include <gtest/gtest.h>

#include "state-fixture.h"

using namespace unity::indicator::datetime;

class AlarmQueueFixture: public StateFixture
{
private:

    typedef StateFixture super;

protected:

    std::vector<std::string> m_triggered;
    std::shared_ptr<WakeupTimer> m_wakeup_timer;
    std::unique_ptr<AlarmQueue> m_watcher;
    std::shared_ptr<RangePlanner> m_range_planner;
    std::shared_ptr<UpcomingPlanner> m_upcoming;

    void SetUp()
    {
        super::SetUp();

        m_wakeup_timer.reset(new MainloopWakeupTimer(m_state->clock));
        m_range_planner.reset(new MockRangePlanner);
        m_upcoming.reset(new UpcomingPlanner(m_range_planner, m_state->clock->localtime()));
        m_watcher.reset(new SimpleAlarmQueue(m_state->clock, m_upcoming, m_wakeup_timer));
        m_watcher->alarm_reached().connect([this](const Appointment& appt){
            m_triggered.push_back(appt.uid);
        });

        EXPECT_TRUE(m_triggered.empty());
    }

    void TearDown()
    {
        m_triggered.clear();
        m_watcher.reset();
        m_upcoming.reset();
        m_range_planner.reset();

        super::TearDown();
    }

    std::vector<Appointment> build_some_appointments()
    {
        const auto now = m_state->clock->localtime();
        const auto tomorrow_begin = now.add_days(1).start_of_day();
        const auto tomorrow_end = tomorrow_begin.add_full(0, 0, 1, 0, 0, -1);

        Appointment a1; // an alarm clock appointment
        a1.color = "red";
        a1.summary = "Alarm";
        a1.summary = "http://www.example.com/";
        a1.uid = "example";
        a1.type = Appointment::UBUNTU_ALARM;
        a1.begin = tomorrow_begin;
        a1.end = tomorrow_end;

        const auto ubermorgen_begin = now.add_days(2).start_of_day();
        const auto ubermorgen_end = ubermorgen_begin.add_full(0, 0, 1, 0, 0, -1);

        Appointment a2; // a non-alarm appointment
        a2.color = "green";
        a2.summary = "Other Text";
        a2.summary = "http://www.monkey.com/";
        a2.uid = "monkey";
        a1.type = Appointment::EVENT;
        a2.begin = ubermorgen_begin;
        a2.end = ubermorgen_end;

        return std::vector<Appointment>({a1, a2});
    }
};

/***
****
***/

TEST_F(AlarmQueueFixture, AppointmentsChanged)
{
    // Add some appointments to the planner.
    // One of these matches our state's localtime, so that should get triggered.
    std::vector<Appointment> a = build_some_appointments();
    a[0].begin = m_state->clock->localtime();
    m_range_planner->appointments().set(a);

    // Confirm that it got fired
    ASSERT_EQ(1, m_triggered.size());
    EXPECT_EQ(a[0].uid, m_triggered[0]);
}


TEST_F(AlarmQueueFixture, TimeChanged)
{
    // Add some appointments to the planner.
    // Neither of these match the state's localtime, so nothing should be triggered.
    std::vector<Appointment> a = build_some_appointments();
    m_range_planner->appointments().set(a);
    EXPECT_TRUE(m_triggered.empty());

    // Set the state's clock to a time that matches one of the appointments().
    // That appointment should get triggered.
g_message ("%s setting clock to %s", G_STRLOC, a[1].begin.format("%F %T").c_str());
    m_mock_state->mock_clock->set_localtime(a[1].begin);
    ASSERT_EQ(1, m_triggered.size());
    EXPECT_EQ(a[1].uid, m_triggered[0]);
}


TEST_F(AlarmQueueFixture, MoreThanOne)
{
    const auto now = m_state->clock->localtime();
    std::vector<Appointment> a = build_some_appointments();
    a[0].begin = a[1].begin = now;
    m_range_planner->appointments().set(a);

    ASSERT_EQ(2, m_triggered.size());
    EXPECT_EQ(a[0].uid, m_triggered[0]);
    EXPECT_EQ(a[1].uid, m_triggered[1]);
}


TEST_F(AlarmQueueFixture, NoDuplicates)
{
    // Setup: add an appointment that gets triggered.
    const auto now = m_state->clock->localtime();
    const std::vector<Appointment> appointments = build_some_appointments();
    std::vector<Appointment> a;
    a.push_back(appointments[0]);
    a[0].begin = now;
    m_range_planner->appointments().set(a);
    ASSERT_EQ(1, m_triggered.size());
    EXPECT_EQ(a[0].uid, m_triggered[0]);

    // Now change the appointment vector by adding one to it.
    // Confirm that the AlarmQueue doesn't re-trigger a[0]
    a.push_back(appointments[1]);
    m_range_planner->appointments().set(a);
    ASSERT_EQ(1, m_triggered.size());
    EXPECT_EQ(a[0].uid, m_triggered[0]);
}
