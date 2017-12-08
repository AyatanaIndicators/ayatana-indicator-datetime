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

#ifndef INDICATOR_DATETIME_APPOINTMENT_H
#define INDICATOR_DATETIME_APPOINTMENT_H

#include <datetime/date-time.h>

#include <string>
#include <vector>

namespace ayatana {
namespace indicator {
namespace datetime {

/**
 * \brief Basic information required to raise a notification about some Appointment.
 */
struct Alarm
{
    std::string text;
    std::string audio_url;
    DateTime time;

    bool operator== (const Alarm& that) const;
};

/**
 * \brief An instance of an appointment; e.g. a calendar event or clock-app alarm
 *
 * @see Planner
 */
struct Appointment
{
public:
    enum Type { EVENT, UBUNTU_ALARM };
    Type type = EVENT;
    bool is_ubuntu_alarm() const { return type == UBUNTU_ALARM; }

    std::string uid;
    std::string color;
    std::string summary;
    std::string activation_url;
    DateTime begin;
    DateTime end;

    std::vector<Alarm> alarms;

    bool operator== (const Appointment& that) const;
};

} // namespace datetime
} // namespace indicator
} // namespace ayatana

#endif // INDICATOR_DATETIME_APPOINTMENT_H
