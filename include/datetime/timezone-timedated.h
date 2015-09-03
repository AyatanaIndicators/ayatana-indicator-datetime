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

#ifndef INDICATOR_DATETIME_TIMEDATED_TIMEZONE_H
#define INDICATOR_DATETIME_TIMEDATED_TIMEZONE_H

#define DEFAULT_FILENAME "/etc/timezone"

#include <datetime/timezone.h> // base class

#include <string> // std::string

namespace unity {
namespace indicator {
namespace datetime {

/**
 * \brief A #Timezone that gets its information from monitoring a file, such as /etc/timezone
 */
class TimedatedTimezone: public Timezone
{
public:
    TimedatedTimezone(std::string filename = DEFAULT_FILENAME);
    ~TimedatedTimezone();

private:
    class Impl;
    friend Impl;
    std::unique_ptr<Impl> impl;

    // we have pointers in here, so disable copying
    TimedatedTimezone(const TimedatedTimezone&) =delete;
    TimedatedTimezone& operator=(const TimedatedTimezone&) =delete;
};

} // namespace datetime
} // namespace indicator
} // namespace unity

#endif // INDICATOR_DATETIME_TIMEDATED_TIMEZONE_H
