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

#ifndef INDICATOR_DATETIME_WAKEUP_TIMER_MAINLOOP_H
#define INDICATOR_DATETIME_WAKEUP_TIMER_MAINLOOP_H

#include <datetime/clock.h>
#include <datetime/wakeup-timer.h>

#include <memory> // std::unique_ptr, std::shared_ptr

namespace unity {
namespace indicator {
namespace datetime {

/***
****
***/

/**
 * \brief a WakeupTimer implemented with g_timeout_add() 
 */
class MainloopWakeupTimer: public WakeupTimer
{
public:
    explicit MainloopWakeupTimer(const std::shared_ptr<Clock>&);
    ~MainloopWakeupTimer();
    void set_wakeup_time (const DateTime&);
    core::Signal<>& timeout();

private:
    MainloopWakeupTimer(const MainloopWakeupTimer&) =delete;
    MainloopWakeupTimer& operator= (const MainloopWakeupTimer&) =delete;
    class Impl;
    std::unique_ptr<Impl> p;
};

/***
****
***/

} // namespace datetime
} // namespace indicator
} // namespace unity

#endif // INDICATOR_DATETIME_WAKEUP_TIMER_MAINLOOP_H
