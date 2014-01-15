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

#include <datetime/settings-live.h>

namespace unity {
namespace indicator {
namespace datetime {

/***
****
***/

LiveSettings::~LiveSettings()
{
    g_clear_object(&m_settings);
}

LiveSettings::LiveSettings():
    m_settings(g_settings_new(SETTINGS_INTERFACE))
{
    g_signal_connect (m_settings, "changed", G_CALLBACK(on_changed), this);

    update_show_clock();
}

void LiveSettings::update_show_clock()
{
    auto val = g_settings_get_boolean(m_settings, SETTINGS_SHOW_CLOCK_S);
    show_clock.set(val);
}

void LiveSettings::on_changed(GSettings* /*settings*/,
                              gchar*       key,
                              gpointer     gself)
{
    static_cast<LiveSettings*>(gself)->update_key(key);
}

void LiveSettings::update_key(const std::string& key)
{
    if (key == SETTINGS_SHOW_CLOCK_S)
    {
        update_show_clock();
    }
}

/***
****
***/

} // namespace datetime
} // namespace indicator
} // namespace unity

#if 0
    else if (!g_strcmp0(key, TIME_FORMAT_S))
    {
    }
    else if (!g_strcmp0(key, SETTINGS_TIME_FORMAT_S))
    {
    }
    else if (!g_strcmp0(key, SETTINGS_SHOW_SECONDS_S))
    {
    }
    else if (!g_strcmp0(key, SETTINGS_SHOW_DAY_S))
    {
    }
    else if (!g_strcmp0(key, SETTINGS_SHOW_DATE_S))
    {
    }
    else if (!g_strcmp0(key, SETTINGS_SHOW_YEAR_S))
    {
    }
    else if (!g_strcmp0(key, SETTINGS_SHOW_CUSTOM_TIME_FORMAT_S))
    {
    }
    else if (!g_strcmp0(key, SETTINGS_SHOW_CALENDAR_S))
    {
    }
    else if (!g_strcmp0(key, SETTINGS_SHOW_WEEK_NUMBERS_S))
    {
    }
    else if (!g_strcmp0(key, SETTINGS_SHOW_EVENTS_S))
    {
    }
    else if (!g_strcmp0(key, SETTINGS_SHOW_LOCATIONS_S))
    {
    }
    else if (!g_strcmp0(key, SETTINGS_TIMEZONE_NAME_S))
    {
    }
    else if (!g_strcmp0(key, SETTINGS_SHOW_DETECTED_S))
    {
    }
    else if (!g_strcmp0(key, SETTINGS_LOCATIONS_S))
    {
    }
#define SETTINGS_SHOW_DETECTED_S        "show-auto-detected-location"
#define SETTINGS_LOCATIONS_S            "locations"
#define SETTINGS_TIMEZONE_NAME_S        "timezone-name"
        zzz
           "show-clock"

void                user_function                      (GSettings *settings,
                                                        gchar     *key,
                                                        gpointer   user_data)      : Has Details

  for (i=0, n=G_N_ELEMENTS(header_settings); i<n; i++)
    {
      g_string_printf (gstr, "changed::%s", header_settings[i]);
      g_signal_connect_swapped (p->settings, gstr->str,
                                G_CALLBACK(rebuild_header_soon), self);
    }


  const char * const calendar_settings[] = {
    SETTINGS_SHOW_CALENDAR_S,
    SETTINGS_SHOW_WEEK_NUMBERS_S
  };
  const char * const appointment_settings[] = {
    SETTINGS_SHOW_EVENTS_S,
    SETTINGS_TIME_FORMAT_S,
    SETTINGS_SHOW_SECONDS_S
  };
  const char * const location_settings[] = {
    SETTINGS_TIME_FORMAT_S,
    SETTINGS_SHOW_SECONDS_S,
    SETTINGS_CUSTOM_TIME_FORMAT_S,
    SETTINGS_SHOW_LOCATIONS_S,
    SETTINGS_LOCATIONS_S,
    SETTINGS_SHOW_DETECTED_S,
    SETTINGS_TIMEZONE_NAME_S
  };
  const char * const time_format_string_settings[] = {
    SETTINGS_TIME_FORMAT_S,
    SETTINGS_SHOW_SECONDS_S,
    SETTINGS_CUSTOM_TIME_FORMAT_S
  };


  /***
  ****  Listen for settings changes
  ***/

  for (i=0, n=G_N_ELEMENTS(header_settings); i<n; i++)
    {
      g_string_printf (gstr, "changed::%s", header_settings[i]);
      g_signal_connect_swapped (p->settings, gstr->str,
                                G_CALLBACK(rebuild_header_soon), self);
    }

}


#ifndef INDICATOR_DATETIME_SETTINGS_LIVE_H
#define INDICATOR_DATETIME_SETTINGS_LIVE_H

#include <datetime/settings.h> // parent class

#include <glib.h> // GSettings

namespace unity {
namespace indicator {
namespace datetime {

/**
 * \brief #Settings implementation which uses GSettings.
 */
class LiveSettings 
{
public:
    LiveSettings();
    virtual ~LiveSettings();

private:
    GSettings* m_settings;

    // we've got a raw pointer here, so disable copying
    LiveSettings(const LiveSettings&) =delete;
    LiveSettings& operator=(const LiveSettings&) =delete;
};


#endif // INDICATOR_DATETIME_SETTINGS_LIVE_H
/*
 * Copyright 2010 Canonical Ltd.
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

#ifndef INDICATOR_DATETIME_SETTINGS_SHARED
#define INDICATOR_DATETIME_SETTINGS_SHARED

typedef enum
{
  TIME_FORMAT_MODE_LOCALE_DEFAULT,
  TIME_FORMAT_MODE_12_HOUR,
  TIME_FORMAT_MODE_24_HOUR,
  TIME_FORMAT_MODE_CUSTOM
}
TimeFormatMode;


#endif // INDICATOR_DATETIME_SETTINGS_SHARED
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

#ifndef INDICATOR_DATETIME_SETTINGS_H
#define INDICATOR_DATETIME_SETTINGS_H

#include <datetime/settings-shared.h>

#include <core/property.h>

namespace unity {
namespace indicator {
namespace datetime {

/**
 * \brief Interface that represents user-configurable settings.
 *
 * See the descriptions in data/com.canonical.indicator.datetime.gschema.xml
 * For more information.
 */
class Settings 
{
public:
    Settings() =default;
    virtual ~Settings =default;

    core::Property<TimeFormatMode> time_format_mode;
    core::Property<bool> show_clock;
    core::Property<bool> show_day;
    core::Property<bool> show_year;
    core::Property<bool> show_seconds;
    core::Property<std::string> custom_time_format;
    core::Property<bool> show_calendar;
    core::Property<bool> show_events;
    core::Property<bool> show_locations;
    core::Property<bool> show_auto_detected_location;
    core::Property<std::vector<std::string>> locations;
    core::Property<std::string> timezone_name;
};

#endif // INDICATOR_DATETIME_SETTINGS_H
#endif
