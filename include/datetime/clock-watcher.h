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

#ifndef INDICATOR_DATETIME_CLOCK_WATCHER_H
#define INDICATOR_DATETIME_CLOCK_WATCHER_H

#include <datetime/state.h>
#include <datetime/appointment.h>

#include <core/signal.h>

#include <memory>
#include <set>
#include <string>

namespace unity {
namespace indicator {
namespace datetime {


/**
 * \brief Watches the clock and appointments to notify when an
 *        appointment's time is reached.
 */
class ClockWatcher
{
public:
    ClockWatcher() =default;
    virtual ~ClockWatcher() =default;
    virtual core::Signal<const Appointment&>& alarm_reached() = 0;
};


/**
 * \brief A #ClockWatcher implementation 
 */
class ClockWatcherImpl: public ClockWatcher
{
public:
    ClockWatcherImpl(const std::shared_ptr<const State>& state);
    ~ClockWatcherImpl() =default;
    core::Signal<const Appointment&>& alarm_reached();

private:
    void pulse();
    std::set<std::string> m_triggered;
    std::shared_ptr<const State> m_state;
    core::Signal<const Appointment&> m_alarm_reached;
};


} // namespace datetime
} // namespace indicator
} // namespace unity

#endif // INDICATOR_DATETIME_CLOCK_WATCHER_H
