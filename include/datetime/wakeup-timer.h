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

#ifndef INDICATOR_DATETIME_WAKEUP_TIMER_H
#define INDICATOR_DATETIME_WAKEUP_TIMER_H

#include <datetime/date-time.h>

#include <core/signal.h>

namespace ayatana {
namespace indicator {
namespace datetime {

/***
****
***/

/**
 * \brief A one-shot timer that emits a signal when its timeout is reached.
 */
class WakeupTimer
{
public:
    WakeupTimer() =default;
    virtual ~WakeupTimer() =default;
    virtual void set_wakeup_time (const DateTime&) =0;
    virtual core::Signal<>& timeout() =0;
};

/***
****
***/

} // namespace datetime
} // namespace indicator
} // namespace ayatana

#endif // INDICATOR_DATETIME_WAKEUP_TIMER_H
