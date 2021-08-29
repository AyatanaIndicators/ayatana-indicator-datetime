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
 *   Ted Gould <ted@canonical.com>
 *   Charles Kerr <charles.kerr@canonical.com>
 */

#ifndef INDICATOR_DATETIME_DBUS_SHARED_H
#define INDICATOR_DATETIME_DBUS_SHARED_H

#define BUS_DATETIME_NAME    "org.ayatana.indicator.datetime"
#define BUS_DATETIME_PATH    "/org/ayatana/indicator/datetime"

#define BUS_POWERD_NAME      "com.lomiri.Repowerd"
#define BUS_POWERD_PATH      "/com/lomiri/Repowerd"
#define BUS_POWERD_INTERFACE "com.lomiri.Repowerd"

namespace Bus
{
    namespace Timedate1
    {
        static constexpr char const * BUSNAME {"org.freedesktop.timedate1"};
        static constexpr char const * ADDR {"/org/freedesktop/timedate1"};
        static constexpr char const * IFACE {"org.freedesktop.timedate1"};

        namespace Properties
        {
            static constexpr char const * TIMEZONE {"Timezone"};
        }

        namespace Methods
        {
            static constexpr char const * SET_TIMEZONE {"SetTimezone"};
        }
    }

    namespace Properties
    {
        static constexpr char const * IFACE {"org.freedesktop.DBus.Properties"};

        namespace Methods
        {
            static constexpr char const * GET {"Get"};
        }

        namespace Signals
        {
            static constexpr char const * PROPERTIES_CHANGED {"PropertiesChanged"};
        }
    }
}

#endif /* INDICATOR_DATETIME_DBUS_SHARED_H */
