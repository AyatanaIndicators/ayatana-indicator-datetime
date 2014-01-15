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
#include <datetime/settings-shared.h>

#include <glib/gi18n-lib.h>
#include <gio/gio.h>

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
    g_strstrip(location_dup);

    gchar* first;
    if((first = strchr(location_dup, ' ')))
        *first = '\0';

    if(zone)
    {
        *zone = location_dup;
    }

    if(name != nullptr)
    {
        gchar* after = first ? g_strstrip(first + 1) : nullptr;

        if(after && *after)
        {
            *name = g_strdup(after);
        }
        else // make the name from zone
        {
            gchar * chr = strrchr(location_dup, '/');
            after = g_strdup(chr ? chr + 1 : location_dup);

            // replace underscores with spaces
            for(chr=after; chr && *chr; chr++)
                if(*chr == '_')
                    *chr = ' ';

            *name = after;
        }
    }
}

gchar*
get_current_zone_name(const gchar* location, GSettings* settings)
{
    gchar* new_zone;
    gchar* new_name;
    split_settings_location(location, &new_zone, &new_name);

    auto tz_name = g_settings_get_string(settings, SETTINGS_TIMEZONE_NAME_S);
    gchar* old_zone;
    gchar* old_name;
    split_settings_location(tz_name, &old_zone, &old_name);
    g_free(tz_name);

    /* new_name is always just a sanitized version of a timezone.
       old_name is potentially a saved "pretty" version of a timezone name from
       geonames.  So we prefer to use it if available and the zones match. */

    gchar* rv;
    if (g_strcmp0(old_zone, new_zone) == 0)
    {
        rv = old_name;
        old_name = nullptr;
    }
    else
    {
        rv = new_name;
        new_name = nullptr;
    }

    g_free(new_zone);
    g_free(old_zone);
    g_free(new_name);
    g_free(old_name);
    return rv;
}

gchar* generate_full_format_string_at_time(GDateTime* now, GDateTime* then)
{
    using unity::indicator::datetime::Clock;
    using unity::indicator::datetime::DateTime;
    using unity::indicator::datetime::MockClock;
    using unity::indicator::datetime::DesktopFormatter;

    std::shared_ptr<Clock> clock(new MockClock(DateTime(now)));
    DesktopFormatter formatter(clock);
    return g_strdup(formatter.getRelativeFormat(then).c_str());
}

