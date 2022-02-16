/*
 * Copyright 2013 Canonical Ltd.
 * Copyright 2022 Robert Tari
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
 *   Robert Tari <robert@tari.in>
 */

#ifndef INDICATOR_DATETIME_GEOCLUE_TIMEZONE_H
#define INDICATOR_DATETIME_GEOCLUE_TIMEZONE_H

#include <datetime/timezone.h> // base class

namespace ayatana {
namespace indicator {
namespace datetime {

/**
 * \brief A #Timezone that gets its information from asking GeoClue
 */
class GeoclueTimezone: public Timezone
{
public:
    GeoclueTimezone();
    ~GeoclueTimezone();

private:
    class Impl;
    std::unique_ptr<Impl> impl;

    // we've got pointers in here, so don't allow copying
    GeoclueTimezone(const GeoclueTimezone&) =delete;
    GeoclueTimezone& operator=(const GeoclueTimezone&) =delete;
};


} // namespace datetime
} // namespace indicator
} // namespace ayatana

#endif // INDICATOR_DATETIME_GEOCLUE_TIMEZONE_H

