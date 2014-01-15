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

#ifndef INDICATOR_DATETIME_SETTINGS_LOCATIONS_H
#define INDICATOR_DATETIME_SETTINGS_LOCATIONS_H

#include <datetime/locations.h> // base class

#include <glib.h>
#include <gio/gio.h>

namespace unity {
namespace indicator {
namespace datetime {

class Timezones;

/**
 * \brief Settings implentation which builds its list from the
 *        user's GSettings and from the Timezones passed in the ctor.
 */
class SettingsLocations: public Locations
{
public:
    /**
     * @param[in] schemaId the settings schema to load 
     * @param[in] timezones the timezones to always show first in the list
     */
    SettingsLocations (const std::string& schemaId,
                       const std::shared_ptr<Timezones>& timezones);

protected:
    std::unique_ptr<GSettings,std::function<void(GSettings*)>> m_settings;
    std::shared_ptr<Timezones> m_timezones;

private:
    static void onSettingsChanged (gpointer gself);
    void reload();
};

} // namespace datetime
} // namespace indicator
} // namespace unity

#endif // INDICATOR_DATETIME_SETTINGS_LOCATIONS_H
