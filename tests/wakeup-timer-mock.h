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
 *   Charles Kerr <charles.kerr@canonical.com>
 */

#ifndef INDICATOR_DATETIME_WAKEUP_TIMER_MOCK_H
#define INDICATOR_DATETIME_WAKEUP_TIMER_MOCK_H

#include <datetime/clock.h>
#include <datetime/wakeup-timer.h>

namespace unity {
namespace indicator {
namespace datetime {

/***
****
***/

/**
 * \brief A one-shot timer that emits a signal when its timeout is reached.
 */
class MockWakeupTimer: public WakeupTimer
{
public:
    explicit MockWakeupTimer(const std::shared_ptr<Clock>& clock):
        m_clock(clock)
    {
        m_clock->minute_changed.connect([this](){
            test_for_wakeup();
        });
    }

    virtual ~MockWakeupTimer() =default;

    virtual void set_wakeup_time (const DateTime& wakeup_time) override {
        m_wakeup_time = wakeup_time;
        test_for_wakeup();
    }

    virtual core::Signal<>& timeout() override { return m_timeout; }

private:

    void test_for_wakeup()
    {
        if (DateTime::is_same_minute(m_clock->localtime(), m_wakeup_time))
            m_timeout();
    }

    core::Signal<> m_timeout;
    const std::shared_ptr<Clock>& m_clock;
    DateTime m_wakeup_time;
};

/***
****
***/

} // namespace datetime
} // namespace indicator
} // namespace unity

#endif // INDICATOR_DATETIME_WAKEUP_TIMER_MOCK_H
