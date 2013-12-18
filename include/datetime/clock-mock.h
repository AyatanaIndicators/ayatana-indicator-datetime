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

#ifndef INDICATOR_DATETIME_CLOCK_MOCK_H
#define INDICATOR_DATETIME_CLOCK_MOCK_H

#include <datetime/clock.h>

namespace unity {
namespace indicator {
namespace datetime {

/***
****
***/

class MockClock: public Clock
{
public:

    MockClock(GDateTime * dt) { setLocaltime(dt); }

    ~MockClock() {
        g_clear_pointer(&localtime_, g_date_time_unref);
    }

    GDateTime* localtime() const {
        g_assert (localtime_ != nullptr);
        return g_date_time_ref(localtime_);
    }

    void setLocaltime(GDateTime* dt) {
        g_clear_pointer(&localtime_, g_date_time_unref);
        localtime_ = g_date_time_ref(dt);
        skewDetected();
    }

private:

    GDateTime * localtime_ = nullptr;
};

} // namespace datetime
} // namespace indicator
} // namespace unity

#endif // INDICATOR_DATETIME_CLOCK_MOCK_H
