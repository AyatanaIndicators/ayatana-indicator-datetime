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

#include <datetime/timezones-live.h>
#include <glib.h>

namespace unity {
namespace indicator {
namespace datetime {

LiveTimezones::LiveTimezones (const std::string& filename):
    file_ (filename)
{
    file_.timezone.changed().connect([this](const std::string&){updateTimezones();});

    geolocationEnabled.changed().connect([this](bool){updateGeolocation();});
    updateGeolocation();

    updateTimezones();
}

void
LiveTimezones::updateGeolocation()
{
    geo_.reset();

    if (geolocationEnabled.get())
    {
        GeoclueTimezone * geo = new GeoclueTimezone();
        geo->timezone.changed().connect([this](const std::string&){updateTimezones();});
        geo_.reset(geo);
    }
}

void
LiveTimezones::updateTimezones()
{
    const std::string a = file_.timezone.get();
    const std::string b = geo_ ? geo_->timezone.get() : "";

    timezone.set (a.empty() ? b : a);

    std::set<std::string> zones;
    if (!a.empty())
        zones.insert(a);
    if (!b.empty())
        zones.insert(b);
    timezones.set(zones);
}

} // namespace datetime
} // namespace indicator
} // namespace unity
