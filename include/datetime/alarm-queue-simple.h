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

#ifndef INDICATOR_DATETIME_ALARM_QUEUE_SIMPLE_H
#define INDICATOR_DATETIME_ALARM_QUEUE_SIMPLE_H

#include <datetime/alarm-queue.h>
#include <datetime/clock.h>
#include <datetime/planner.h>
#include <datetime/wakeup-timer.h>

namespace unity {
namespace indicator {
namespace datetime {

/**
 * \brief A #AlarmQueue implementation 
 */
class SimpleAlarmQueue: public AlarmQueue
{
public:
    SimpleAlarmQueue(const std::shared_ptr<Clock>& clock,
                     const std::shared_ptr<Planner>& upcoming_planner,
                     const std::shared_ptr<WakeupTimer>& timer);
    ~SimpleAlarmQueue();
    core::Signal<const Appointment&>& alarm_reached();

private:
    void requeue();
    bool find_next_alarm(Appointment& setme) const;
    std::vector<Appointment> find_current_alarms() const;
    void check_alarms();

    std::set<std::string> m_triggered;
    const std::shared_ptr<Clock> m_clock;
    const std::shared_ptr<Planner> m_planner;
    const std::shared_ptr<WakeupTimer> m_timer;
    core::Signal<const Appointment&> m_alarm_reached;
    DateTime m_time;
};


} // namespace datetime
} // namespace indicator
} // namespace unity

#endif // INDICATOR_DATETIME_ALARM_QUEUE_H
