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

namespace ayatana {
namespace indicator {
namespace datetime {

/***
****
***/

LiveSettings::~LiveSettings()
{
    g_clear_object(&m_settings_general_notification);
    g_clear_object(&m_settings_cal_notification);
    g_clear_object(&m_settings);
}

LiveSettings::LiveSettings():
    m_settings(g_settings_new(SETTINGS_INTERFACE))
{
    g_signal_connect (m_settings, "changed", G_CALLBACK(on_changed_ccid), this);

    // init the Properties from the GSettings backend
    update_custom_time_format();
    update_locations();
    update_show_calendar();
    update_show_date();
    update_show_day();
    update_show_detected_locations();
    update_show_events();
    update_show_alarms();
    update_show_locations();
    update_show_seconds();
    update_show_week_numbers();
    update_show_year();
    update_time_format_mode();
    update_timezone_name();
    update_calendar_sound();
    update_calendar_volume();
    update_alarm_sound();
    update_alarm_volume();
    update_alarm_duration();
    update_alarm_haptic();
    update_snooze_duration();

    GSettingsSchemaSource *pSource = g_settings_schema_source_get_default();
    GSettingsSchema *pSchema = g_settings_schema_source_lookup(pSource, SETTINGS_NOTIFY_SCHEMA_ID, TRUE);

    if (pSchema != NULL)
    {
        m_settings_cal_notification = g_settings_new_with_path(SETTINGS_NOTIFY_SCHEMA_ID, SETTINGS_NOTIFY_CALENDAR_PATH);
        m_settings_general_notification = g_settings_new(SETTINGS_NOTIFY_APPS_SCHEMA_ID);
        g_signal_connect (m_settings_cal_notification,     "changed", G_CALLBACK(on_changed_cal_notification), this);
        g_signal_connect (m_settings_general_notification, "changed", G_CALLBACK(on_changed_general_notification), this);
        update_cal_notification_enabled();
        update_cal_notification_sounds();
        update_cal_notification_vibrations();
        update_cal_notification_bubbles();
        update_cal_notification_list();
        update_vibrate_silent_mode();
    }
    else
    {
        m_settings_cal_notification = NULL;
        m_settings_general_notification = NULL;
    }

    // now listen for clients to change the properties s.t. we can sync update GSettings

    custom_time_format.changed().connect([this](const std::string& value){
        g_settings_set_string(m_settings, SETTINGS_CUSTOM_TIME_FORMAT_S, value.c_str());
    });

    locations.changed().connect([this](const std::vector<std::string>& value){
        const int n = value.size();
        gchar** strv = g_new0(gchar*, n+1);
        for(int i=0; i<n; i++)
            strv[i] = const_cast<char*>(value[i].c_str());
        g_settings_set_strv(m_settings, SETTINGS_LOCATIONS_S, strv);
        g_free(strv);
    });

    show_calendar.changed().connect([this](bool value){
        g_settings_set_boolean(m_settings, SETTINGS_SHOW_CALENDAR_S, value);
    });

    show_date.changed().connect([this](bool value){
        g_settings_set_boolean(m_settings, SETTINGS_SHOW_DATE_S, value);
    });

    show_day.changed().connect([this](bool value){
        g_settings_set_boolean(m_settings, SETTINGS_SHOW_DAY_S, value);
    });

    show_detected_location.changed().connect([this](bool value){
        g_settings_set_boolean(m_settings, SETTINGS_SHOW_DETECTED_S, value);
    });

    show_events.changed().connect([this](bool value){
        g_settings_set_boolean(m_settings, SETTINGS_SHOW_EVENTS_S, value);
    });

    show_alarms.changed().connect([this](bool value){
        g_settings_set_boolean(m_settings, SETTINGS_SHOW_ALARMS_S, value);
    });

    show_locations.changed().connect([this](bool value){
        g_settings_set_boolean(m_settings, SETTINGS_SHOW_LOCATIONS_S, value);
    });

    show_seconds.changed().connect([this](bool value){
        g_settings_set_boolean(m_settings, SETTINGS_SHOW_SECONDS_S, value);
    });

    show_week_numbers.changed().connect([this](bool value){
        g_settings_set_boolean(m_settings, SETTINGS_SHOW_WEEK_NUMBERS_S, value);
    });

    show_year.changed().connect([this](bool value){
        g_settings_set_boolean(m_settings, SETTINGS_SHOW_YEAR_S, value);
    });

    time_format_mode.changed().connect([this](TimeFormatMode value){
        g_settings_set_enum(m_settings, SETTINGS_TIME_FORMAT_S, gint(value));
    });

    timezone_name.changed().connect([this](const std::string& value){
        g_settings_set_string(m_settings, SETTINGS_TIMEZONE_NAME_S, value.c_str());
    });

    calendar_sound.changed().connect([this](const std::string& value){
        g_settings_set_string(m_settings, SETTINGS_CALENDAR_SOUND_S, value.c_str());
    });

    calendar_volume.changed().connect([this](unsigned int value){
        g_settings_set_uint(m_settings, SETTINGS_CALENDAR_VOLUME_S, value);
    });

    alarm_sound.changed().connect([this](const std::string& value){
        g_settings_set_string(m_settings, SETTINGS_ALARM_SOUND_S, value.c_str());
    });

    alarm_volume.changed().connect([this](unsigned int value){
        g_settings_set_uint(m_settings, SETTINGS_ALARM_VOLUME_S, value);
    });

    alarm_duration.changed().connect([this](unsigned int value){
        g_settings_set_uint(m_settings, SETTINGS_ALARM_DURATION_S, value);
    });

    alarm_haptic.changed().connect([this](const std::string& value){
        g_settings_set_string(m_settings, SETTINGS_ALARM_HAPTIC_S, value.c_str());
    });

    snooze_duration.changed().connect([this](unsigned int value){
        g_settings_set_uint(m_settings, SETTINGS_SNOOZE_DURATION_S, value);
    });

    cal_notification_enabled.changed().connect([this](bool value){
        g_settings_set_boolean(m_settings_cal_notification, SETTINGS_NOTIFY_ENABLED_KEY, value);
    });

    cal_notification_sounds.changed().connect([this](bool value){
        g_settings_set_boolean(m_settings_cal_notification, SETTINGS_NOTIFY_SOUNDS_KEY, value);
    });

    cal_notification_vibrations.changed().connect([this](bool value){
        g_settings_set_boolean(m_settings_cal_notification, SETTINGS_NOTIFY_VIBRATIONS_KEY, value);
    });

    cal_notification_bubbles.changed().connect([this](bool value){
        g_settings_set_boolean(m_settings_cal_notification, SETTINGS_NOTIFY_BUBBLES_KEY, value);
    });

    cal_notification_list.changed().connect([this](bool value){
        g_settings_set_boolean(m_settings_cal_notification, SETTINGS_NOTIFY_LIST_KEY, value);
    });

    vibrate_silent_mode.changed().connect([this](bool value){
        g_settings_set_boolean(m_settings_general_notification, SETTINGS_VIBRATE_SILENT_KEY, value);
    });
}

/***
****
***/

void LiveSettings::update_custom_time_format()
{
    auto val = g_settings_get_string(m_settings, SETTINGS_CUSTOM_TIME_FORMAT_S);
    custom_time_format.set(val);
    g_free(val);
}

void LiveSettings::update_locations()
{
    auto strv = g_settings_get_strv(m_settings, SETTINGS_LOCATIONS_S);
    std::vector<std::string> l;
    for(int i=0; strv && strv[i]; i++)
        l.push_back(strv[i]);
    g_strfreev(strv);
    locations.set(l);
}

void LiveSettings::update_show_calendar()
{
    const auto val = g_settings_get_boolean(m_settings, SETTINGS_SHOW_CALENDAR_S);
    show_calendar.set(val);
}

void LiveSettings::update_show_date()
{
    show_date.set(g_settings_get_boolean(m_settings, SETTINGS_SHOW_DATE_S));
}

void LiveSettings::update_show_day()
{
    show_day.set(g_settings_get_boolean(m_settings, SETTINGS_SHOW_DAY_S));
}

void LiveSettings::update_show_detected_locations()
{
    const auto val = g_settings_get_boolean(m_settings, SETTINGS_SHOW_DETECTED_S);
    show_detected_location.set(val);
}

void LiveSettings::update_show_events()
{
    const auto val = g_settings_get_boolean(m_settings, SETTINGS_SHOW_EVENTS_S);
    show_events.set(val);
}

void LiveSettings::update_show_alarms()
{
    const auto val = g_settings_get_boolean(m_settings, SETTINGS_SHOW_ALARMS_S);
    show_alarms.set(val);
}

void LiveSettings::update_show_locations()
{
    const auto val = g_settings_get_boolean(m_settings, SETTINGS_SHOW_LOCATIONS_S);
    show_locations.set(val);
}

void LiveSettings::update_show_seconds()
{
    show_seconds.set(g_settings_get_boolean(m_settings, SETTINGS_SHOW_SECONDS_S));
}

void LiveSettings::update_show_week_numbers()
{
    const auto val = g_settings_get_boolean(m_settings, SETTINGS_SHOW_WEEK_NUMBERS_S);
    show_week_numbers.set(val);
}

void LiveSettings::update_show_year()
{
    show_year.set(g_settings_get_boolean(m_settings, SETTINGS_SHOW_YEAR_S));
}

void LiveSettings::update_time_format_mode()
{
    time_format_mode.set((TimeFormatMode)g_settings_get_enum(m_settings, SETTINGS_TIME_FORMAT_S));
}

void LiveSettings::update_timezone_name()
{
    auto val = g_settings_get_string(m_settings, SETTINGS_TIMEZONE_NAME_S);
    timezone_name.set(val);
    g_free(val);
}

void LiveSettings::update_calendar_sound()
{
    auto val = g_settings_get_string(m_settings, SETTINGS_CALENDAR_SOUND_S);
    calendar_sound.set(val);
    g_free(val);
}

void LiveSettings::update_calendar_volume()
{
    calendar_volume.set(g_settings_get_uint(m_settings, SETTINGS_CALENDAR_VOLUME_S));
}

void LiveSettings::update_alarm_sound()
{
    auto val = g_settings_get_string(m_settings, SETTINGS_ALARM_SOUND_S);
    alarm_sound.set(val);
    g_free(val);
}

void LiveSettings::update_alarm_volume()
{
    alarm_volume.set(g_settings_get_uint(m_settings, SETTINGS_ALARM_VOLUME_S));
}

void LiveSettings::update_alarm_duration()
{
    alarm_duration.set(g_settings_get_uint(m_settings, SETTINGS_ALARM_DURATION_S));
}

void LiveSettings::update_alarm_haptic()
{
    auto val = g_settings_get_string(m_settings, SETTINGS_ALARM_HAPTIC_S);
    alarm_haptic.set(val);
    g_free(val);
}

void LiveSettings::update_snooze_duration()
{
    snooze_duration.set(g_settings_get_uint(m_settings, SETTINGS_SNOOZE_DURATION_S));
}

void LiveSettings::update_cal_notification_enabled()
{
    cal_notification_enabled.set(g_settings_get_boolean(m_settings_cal_notification, SETTINGS_NOTIFY_ENABLED_KEY));
}

void LiveSettings::update_cal_notification_sounds()
{
    cal_notification_sounds.set(g_settings_get_boolean(m_settings_cal_notification, SETTINGS_NOTIFY_SOUNDS_KEY));
}

void LiveSettings::update_cal_notification_vibrations()
{
    cal_notification_vibrations.set(g_settings_get_boolean(m_settings_cal_notification, SETTINGS_NOTIFY_VIBRATIONS_KEY));
}

void LiveSettings::update_cal_notification_bubbles()
{
    cal_notification_bubbles.set(g_settings_get_boolean(m_settings_cal_notification, SETTINGS_NOTIFY_BUBBLES_KEY));
}

void LiveSettings::update_cal_notification_list()
{
    cal_notification_list.set(g_settings_get_boolean(m_settings_cal_notification, SETTINGS_NOTIFY_LIST_KEY));
}

void LiveSettings::update_vibrate_silent_mode()
{
    vibrate_silent_mode.set(g_settings_get_boolean(m_settings_general_notification, SETTINGS_VIBRATE_SILENT_KEY));
}

/***
****
***/

void LiveSettings::on_changed_cal_notification(GSettings* /*settings*/,
                                               gchar*     key,
                                               gpointer   gself)
{
    static_cast<LiveSettings*>(gself)->update_key_cal_notification(key);
}


void LiveSettings::update_key_cal_notification(const std::string& key)
{
    if (key == SETTINGS_NOTIFY_ENABLED_KEY)
        update_cal_notification_enabled();
    else if (key == SETTINGS_NOTIFY_SOUNDS_KEY)
        update_cal_notification_sounds();
    else if (key == SETTINGS_NOTIFY_VIBRATIONS_KEY)
        update_cal_notification_vibrations();
    else if (key == SETTINGS_NOTIFY_BUBBLES_KEY)
        update_cal_notification_bubbles();
    else if (key == SETTINGS_NOTIFY_LIST_KEY)
        update_cal_notification_list();
}

/***
****
***/

void LiveSettings::on_changed_general_notification(GSettings* /*settings*/,
                                                   gchar*     key,
                                                   gpointer   gself)
{
    static_cast<LiveSettings*>(gself)->update_key_general_notification(key);
}

void LiveSettings::update_key_general_notification(const std::string& key)
{
    if (key == SETTINGS_VIBRATE_SILENT_KEY)
        update_vibrate_silent_mode();
}

/***
****
***/

void LiveSettings::on_changed_ccid(GSettings* /*settings*/,
                                   gchar*       key,
                                   gpointer     gself)
{
    static_cast<LiveSettings*>(gself)->update_key_ccid(key);
}

void LiveSettings::update_key_ccid(const std::string& key)
{
    if (key == SETTINGS_LOCATIONS_S)
        update_locations();
    else if (key == SETTINGS_TIME_FORMAT_S)
        update_time_format_mode();
    else if (key == SETTINGS_SHOW_SECONDS_S)
        update_show_seconds();
    else if (key == SETTINGS_SHOW_DAY_S)
        update_show_day();
    else if (key == SETTINGS_SHOW_DATE_S)
        update_show_date();
    else if (key == SETTINGS_SHOW_YEAR_S)
        update_show_year();
    else if (key == SETTINGS_CUSTOM_TIME_FORMAT_S)
        update_custom_time_format();
    else if (key == SETTINGS_SHOW_CALENDAR_S)
        update_show_calendar();
    else if (key == SETTINGS_SHOW_WEEK_NUMBERS_S)
        update_show_week_numbers();
    else if (key == SETTINGS_SHOW_EVENTS_S)
        update_show_events();
    else if (key == SETTINGS_SHOW_ALARMS_S)
        update_show_alarms();
    else if (key == SETTINGS_SHOW_LOCATIONS_S)
        update_show_locations();
    else if (key == SETTINGS_SHOW_DETECTED_S)
        update_show_detected_locations();
    else if (key == SETTINGS_TIMEZONE_NAME_S)
        update_timezone_name();
    else if (key == SETTINGS_CALENDAR_SOUND_S)
        update_calendar_sound();
    else if (key == SETTINGS_CALENDAR_VOLUME_S)
        update_calendar_volume();
    else if (key == SETTINGS_ALARM_SOUND_S)
        update_alarm_sound();
    else if (key == SETTINGS_ALARM_VOLUME_S)
        update_alarm_volume();
    else if (key == SETTINGS_ALARM_DURATION_S)
        update_alarm_duration();
    else if (key == SETTINGS_ALARM_HAPTIC_S)
        update_alarm_haptic();
    else if (key == SETTINGS_SNOOZE_DURATION_S)
        update_snooze_duration();
}

/***
****
***/

} // namespace datetime
} // namespace indicator
} // namespace ayatana
