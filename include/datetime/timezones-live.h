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

#ifndef INDICATOR_DATETIME_LIVE_TIMEZONES_H
#define INDICATOR_DATETIME_LIVE_TIMEZONES_H

#include <datetime/settings.h>
#include <datetime/timezones.h>
#include <datetime/timezone-geoclue.h>

#include <memory> // shared_ptr<>

namespace ayatana {
namespace indicator {
namespace datetime {

/**
 * \brief #Timezones object that uses a #TimedatedTimezone and #GeoclueTimezone
 *        to detect what timezone we're in
 */
class LiveTimezones: public Timezones
{
public:
    LiveTimezones(const std::shared_ptr<const Settings>& settings, const std::shared_ptr<Timezone>& primary_timezone);

private:
    void update_geolocation();
    void update_timezones();

    std::shared_ptr<Timezone> m_primary_timezone;
    std::shared_ptr<const Settings> m_settings;
    std::shared_ptr<GeoclueTimezone> m_geo;
};

} // namespace datetime
} // namespace indicator
} // namespace ayatana

#endif // INDICATOR_DATETIME_LIVE_TIMEZONES_H
