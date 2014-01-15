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

/**
 * \brief A clock that uses a client-provided time instead of the system time.
 */
class MockClock: public Clock
{
public:
    MockClock(const DateTime& dt): m_localtime(dt) {}
    ~MockClock() =default;

    DateTime localtime() const { return m_localtime; }

    void set_localtime(const DateTime& dt) {
        const auto old_day = m_localtime.day_of_year();
        m_localtime = dt;
        skewDetected();
        const auto new_day = m_localtime.day_of_year();
        if (old_day != new_day)
            dateChanged();
    }

private:
    DateTime m_localtime;
};

} // namespace datetime
} // namespace indicator
} // namespace unity

#endif // INDICATOR_DATETIME_CLOCK_MOCK_H
