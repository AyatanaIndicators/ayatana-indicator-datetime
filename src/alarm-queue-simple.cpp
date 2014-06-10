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

#include <cmath>

namespace unity {
namespace indicator {
namespace datetime {

/***
****  Public API
***/

SimpleAlarmQueue::SimpleAlarmQueue(const std::shared_ptr<Clock>& clock,
                                   const std::shared_ptr<Planner>& planner,
                                   const std::shared_ptr<WakeupTimer>& timer):
    m_clock(clock),
    m_planner(planner),
    m_timer(timer),
    m_datetime(clock->localtime())
{
    m_planner->appointments().changed().connect([this](const std::vector<Appointment>&){
        g_debug("AlarmQueue %p calling requeue() due to appointments changed", this);
        requeue();
    });

    m_clock->minute_changed.connect([=]{
        const auto now = m_clock->localtime();
        constexpr auto skew_threshold_usec = int64_t{90} * G_USEC_PER_SEC;
        const bool clock_jumped = std::abs(now - m_datetime) > skew_threshold_usec;
        m_datetime = now;
        if (clock_jumped) {
            g_debug("AlarmQueue %p calling requeue() due to clock skew", this);
            requeue();
        }
    });

    m_timer->timeout().connect([this](){
        g_debug("AlarmQueue %p calling requeue() due to timeout", this);
        requeue();
    });

    requeue();
}

SimpleAlarmQueue::~SimpleAlarmQueue()
{
}

core::Signal<const Appointment&>& SimpleAlarmQueue::alarm_reached()
{
    return m_alarm_reached;
}

/***
****
***/

void SimpleAlarmQueue::requeue()
{
    // kick any current alarms
    for (auto current : find_current_alarms())
    {
        m_triggered.insert(current.uid);
        m_alarm_reached(current);
    }

    // idle until the next alarm
    Appointment next;
    if (find_next_alarm(next))
    {
        g_debug ("setting timer to wake up for next appointment '%s' at %s", 
                 next.summary.c_str(),
                 next.begin.format("%F %T").c_str());

        m_timer->set_wakeup_time(next.begin);
    }
}

// find the next alarm that will kick now or in the future
bool SimpleAlarmQueue::find_next_alarm(Appointment& setme) const
{
    bool found = false;
    Appointment tmp;
    const auto now = m_clock->localtime();
    const auto beginning_of_minute = now.add_full (0, 0, 0, 0, 0, -now.seconds());

    for(const auto& walk : m_planner->appointments().get())
    {
        if (m_triggered.count(walk.uid)) // did we already use this one?
            continue;

        if (walk.begin < beginning_of_minute) // has this one already passed?
            continue;

        if (found && (tmp.begin < walk.begin)) // do we already have a better match?
            continue;

        tmp = walk;
        found = true;
    }

    if (found)
      setme = tmp;

    return found;
}

// find the alarm(s) that should kick right now
std::vector<Appointment> SimpleAlarmQueue::find_current_alarms() const
{
    std::vector<Appointment> appointments;

    const auto now = m_clock->localtime();

    for(const auto& walk : m_planner->appointments().get())
    {
        if (m_triggered.count(walk.uid)) // did we already use this one?
            continue;
        if (!DateTime::is_same_minute(now, walk.begin))
            continue;

        appointments.push_back(walk);
    }

    return appointments;
}

/***
****
***/

} // namespace datetime
} // namespace indicator
} // namespace unity
