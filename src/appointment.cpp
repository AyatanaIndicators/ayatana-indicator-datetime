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

#include <datetime/appointment.h>

namespace ayatana {
namespace indicator {
namespace datetime {

/****
*****
****/

Alarm::Alarm()
    : type(Alarm::None)
{
}

Alarm::Alarm(int type_, const std::string &text_, const std::string& audio_url_, const DateTime &time_)
    :  type(type_), text(text_), audio_url(audio_url_), time(time_)
{
}

bool Alarm::operator==(const Alarm& that) const
{
  return (type==that.type)
      && (text==that.text)
      && (audio_url==that.audio_url)
          && (this->time==that.time);
}

bool Appointment::operator==(const Appointment& that) const
{
    return (type==that.type)
        && (uid==that.uid)
        && (color==that.color)
        && (summary==that.summary)
        && (begin==that.begin)
        && (end==that.end)
        && (alarms==that.alarms);
}

/****
*****
****/

} // namespace datetime
} // namespace indicator
} // namespace ayatana
