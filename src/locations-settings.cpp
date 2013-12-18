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

#include <datetime/locations-settings.h>

#include <datetime/settings-shared.h>
#include <datetime/timezones.h>
#include <datetime/utils.h>

#include <algorithm> // std::find()

namespace unity {
namespace indicator {
namespace datetime {

SettingsLocations::SettingsLocations (const std::string& schemaId,
                                      const std::shared_ptr<Timezones>& timezones):
    timezones_(timezones)
{
    auto deleter = [&](GSettings* s){g_object_unref(s);};
    settings_ = std::unique_ptr<GSettings,std::function<void(GSettings*)>>(g_settings_new(schemaId.c_str()), deleter);
    const char * keys[] = { "changed::" SETTINGS_LOCATIONS_S,
                            "changed::" SETTINGS_SHOW_LOCATIONS_S };
    for (int i=0, n=G_N_ELEMENTS(keys); i<n; i++)
        g_signal_connect_swapped (settings_.get(), keys[i], G_CALLBACK(onSettingsChanged), this);

    timezones->timezone.changed().connect([this](const std::string&){reload();});
    timezones->timezones.changed().connect([this](const std::set<std::string>&){reload();});

    reload();
}

void
SettingsLocations::onSettingsChanged (gpointer gself)
{
    static_cast<SettingsLocations*>(gself)->reload();
}

void
SettingsLocations::reload()
{
    std::vector<Location> v;

    // add the primary timezone first
    std::string zone = timezones_->timezone.get();
    if (!zone.empty())
    {
        gchar * name = get_current_zone_name (zone.c_str(), settings_.get());
        Location l (zone, name);
        v.push_back (l);
        g_free (name);
    }

    // add the other detected timezones
    for (const auto& zone : timezones_->timezones.get())
    {
        gchar * name = get_current_zone_name (zone.c_str(), settings_.get());
        Location l (zone, name);
        if (std::find (v.begin(), v.end(), l) == v.end())
            v.push_back (l);
        g_free (name);
    }

    // maybe add the user-specified locations
    if (g_settings_get_boolean (settings_.get(), SETTINGS_SHOW_LOCATIONS_S))
    {
        gchar ** user_locations = g_settings_get_strv (settings_.get(), SETTINGS_LOCATIONS_S);

        for (int i=0; user_locations[i]; i++)
        {
            gchar * zone;
            gchar * name;
            split_settings_location (user_locations[i], &zone, &name);
            Location l (zone, name);
            if (std::find (v.begin(), v.end(), l) == v.end())
                v.push_back (l);
            g_free (name);
            g_free (zone);
        }

        g_strfreev (user_locations);
    }

    locations.set (v);
}

} // namespace datetime
} // namespace indicator
} // namespace unity
