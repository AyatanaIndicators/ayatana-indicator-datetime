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

#define SETTINGS_INTERFACE              "org.ayatana.indicator.datetime"
#define SETTINGS_TIME_FORMAT_S          "time-format"
#define SETTINGS_SHOW_SECONDS_S         "show-seconds"
#define SETTINGS_SHOW_DAY_S             "show-day"
#define SETTINGS_SHOW_DATE_S            "show-date"
#define SETTINGS_SHOW_YEAR_S            "show-year"
#define SETTINGS_CUSTOM_TIME_FORMAT_S   "custom-time-format"
#define SETTINGS_SHOW_CALENDAR_S        "show-calendar"
#define SETTINGS_SHOW_WEEK_NUMBERS_S    "show-week-numbers"
#define SETTINGS_SHOW_EVENTS_S          "show-events"
#define SETTINGS_SHOW_LOCATIONS_S       "show-locations"
#define SETTINGS_SHOW_DETECTED_S        "show-auto-detected-location"
#define SETTINGS_LOCATIONS_S            "locations"
#define SETTINGS_TIMEZONE_NAME_S        "timezone-name"
#define SETTINGS_CALENDAR_SOUND_S       "calendar-default-sound"
#define SETTINGS_ALARM_SOUND_S          "alarm-default-sound"
#define SETTINGS_ALARM_VOLUME_S         "alarm-default-volume"
#define SETTINGS_ALARM_DURATION_S       "alarm-duration-minutes"
#define SETTINGS_ALARM_HAPTIC_S         "alarm-haptic-feedback"
#define SETTINGS_SNOOZE_DURATION_S      "snooze-duration-minutes"

#define SETTINGS_NOTIFY_APPS_SCHEMA_ID  "com.lomiri.notifications.settings.applications"
#define SETTINGS_VIBRATE_SILENT_KEY     "vibrate-silent-mode"
#define SETTINGS_NOTIFY_SCHEMA_ID       "com.lomiri.notifications.settings"
#define SETTINGS_NOTIFY_CALENDAR_PATH   "/com/lomiri/NotificationSettings/com.lomiri.calendar/calendar/"
#define SETTINGS_NOTIFY_ENABLED_KEY     "enable-notifications"
#define SETTINGS_NOTIFY_SOUNDS_KEY      "use-sounds-notifications"
#define SETTINGS_NOTIFY_VIBRATIONS_KEY  "use-vibrations-notifications"
#define SETTINGS_NOTIFY_BUBBLES_KEY     "use-bubbles-notifications"
#define SETTINGS_NOTIFY_LIST_KEY        "use-list-notifications"

#endif // INDICATOR_DATETIME_SETTINGS_SHARED
