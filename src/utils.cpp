/* -*- Mode: C; coding: utf-8; indent-tabs-mode: nil; tab-width: 2 -*-

A dialog for setting time and date preferences.

Copyright 2010 Canonical Ltd.

Authors:
    Michael Terry <michael.terry@canonical.com>

This program is free software: you can redistribute it and/or modify it 
under the terms of the GNU General Public License version 3, as published 
by the Free Software Foundation.

This program is distributed in the hope that it will be useful, but 
WITHOUT ANY WARRANTY; without even the implied warranties of 
MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR 
PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along 
with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <datetime/utils.h>

#include <datetime/clock.h>
#include <datetime/clock-mock.h>
#include <datetime/formatter.h>
#include <datetime/settings-live.h>

#include <glib.h>

#include <locale.h>
#include <langinfo.h>
#include <string.h>

/* Check the system locale setting to see if the format is 24-hour
   time or 12-hour time */
gboolean
is_locale_12h()
{
    const char *t_fmt = nl_langinfo(T_FMT);

    static const char *formats_24h[] = {"%H", "%R", "%T", "%OH", "%k"};
    for(const auto& format : formats_24h)
        if(strstr(t_fmt, format) != nullptr)
            return false;

    return true;
}

void
split_settings_location(const gchar* location, gchar** zone, gchar** name)
{
    auto location_dup = g_strdup(location);
    if(location_dup != nullptr)
        g_strstrip(location_dup);

    gchar* first;
    if(location_dup && (first = strchr(location_dup, ' ')))
        *first = '\0';

    if(zone)
        *zone = location_dup;

    if(name != nullptr)
    {
        gchar* after = first ? g_strstrip(first + 1) : nullptr;

        if(after && *after)
        {
            *name = g_strdup(after);
        }
        else if (location_dup) // make the name from zone
        {
            gchar * chr = strrchr(location_dup, '/');
            after = g_strdup(chr ? chr + 1 : location_dup);

            // replace underscores with spaces
            for(chr=after; chr && *chr; chr++)
                if(*chr == '_')
                    *chr = ' ';

            *name = after;
        }
        else
        {
            *name = nullptr;
        }
    }
}

/**
 * Our Locations come from two places: (1) direct user input and (2) ones
 * guessed by the system, such as from geoclue or timedate1.
 *
 * Since the latter only have a timezone (eg, "America/Chicago") and the
 * former have a descriptive name provided by the end user (eg,
 * "America/Chicago Oklahoma City"), this function tries to make a
 * more human-readable name by using the user-provided name if the guessed
 * timezone matches the last one the user manually clicked on.
 * 
 * In the example above, this allows the menuitem for the system-guessed
 * timezone ("America/Chicago") to read "Oklahoma City" after the user clicks
 * on the "Oklahoma City" menuitem.
 */
gchar*
get_beautified_timezone_name(const char* timezone, const char* saved_location)
{
    gchar* zone;
    gchar* name;
    split_settings_location(timezone, &zone, &name);

    gchar* saved_zone;
    gchar* saved_name;
    split_settings_location(saved_location, &saved_zone, &saved_name);

    gchar* rv;
    if (g_strcmp0(zone, saved_zone) == 0)
    {
        rv = saved_name;
        saved_name = nullptr;
    }
    else
    {
        rv = name;
        name = nullptr;
    }

    g_free(zone);
    g_free(name);
    g_free(saved_zone);
    g_free(saved_name);
    return rv;
}

gchar*
get_timezone_name(const gchar* timezone, GSettings* settings)
{
    auto saved_location = g_settings_get_string(settings, SETTINGS_TIMEZONE_NAME_S);
    auto rv = get_beautified_timezone_name(timezone, saved_location);
    g_free(saved_location);
    return rv;
}

using namespace unity::indicator::datetime;

gchar* generate_full_format_string_at_time(GDateTime* now, GDateTime* then)
{
    std::shared_ptr<Clock> clock(new MockClock(DateTime(now)));
    std::shared_ptr<Settings> settings(new LiveSettings);
    DesktopFormatter formatter(clock, settings);
    return g_strdup(formatter.getRelativeFormat(then).c_str());
}

