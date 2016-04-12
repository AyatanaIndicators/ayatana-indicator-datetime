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

#include <datetime/timezone.h> // base class

#include <gio/gio.h> // GDBusConnection*

#include <string> // std::string

namespace ayatana {
namespace indicator {
namespace datetime {

/**
 * \brief A #Timezone that gets its information from org.freedesktop.timedate1
 */
class TimedatedTimezone: public Timezone
{
public:
    TimedatedTimezone(GDBusConnection* connection);
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
} // namespace ayatana

#endif // INDICATOR_DATETIME_TIMEDATED_TIMEZONE_H
