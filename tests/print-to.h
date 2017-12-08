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

#ifndef INDICATOR_DATETIME_TESTS_PRINT_TO
#define INDICATOR_DATETIME_TESTS_PRINT_TO

#include <algorithm>

#include <datetime/appointment.h>

namespace ayatana {
namespace indicator {
namespace datetime {

/***
**** PrintTo() functions for GTest to represent objects as strings
***/

void
PrintTo(const DateTime& datetime, std::ostream* os)
{
    *os << "{time:'" << datetime.format("%F %T %z") << '}';
}

void
PrintTo(const Alarm& alarm, std::ostream* os)
{
    *os << '{';
    *os << "{text:" << alarm.text << '}';
    PrintTo(alarm.time, os);
    *os << '}';
}

void
PrintTo(const Appointment& appointment, std::ostream* os)
{
    *os << '{';

    *os << "{uid:'" << appointment.uid << "'}"
        << "{color:'" << appointment.color << "'}"
        << "{summary:'" << appointment.summary << "'}"
        << "{activation_url:'" << appointment.activation_url << "'}";

    *os << "{begin:";
    PrintTo(appointment.begin, os);
    *os << '}';

    *os << "{end:";
    PrintTo(appointment.end, os);
    *os << '}';

    for(const auto& alarm : appointment.alarms)
        PrintTo(alarm, os);

    *os << '}';
}

} // namespace datetime
} // namespace indicator
} // namespace ayatana

#endif
