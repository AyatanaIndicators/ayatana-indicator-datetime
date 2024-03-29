/*
 * Copyright 2013 Canonical Ltd.
 *
 * Authors:
 *   Charles Kerr <charles.kerr@canonical.com>
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
 */

#include "glib-fixture.h"

#include <datetime/settings-live.h>
#include <datetime/settings-shared.h>

extern "C"
{
    #include <ayatana/common/utils.h>
}

using namespace ayatana::indicator::datetime;

/***
****
***/

class SettingsFixture: public GlibFixture
{
private:
    typedef GlibFixture super;

protected:

    std::shared_ptr<LiveSettings> m_live;
    std::shared_ptr<Settings> m_settings;
    GSettings * m_gsettings {};
    GSettings * m_gsettings_cal_notification {};

    void SetUp() override
    {
        super::SetUp();

        m_gsettings = g_settings_new(SETTINGS_INTERFACE);
        GSettingsSchemaSource *pSource = g_settings_schema_source_get_default();
        GSettingsSchema *pSchema = g_settings_schema_source_lookup(pSource, SETTINGS_NOTIFY_SCHEMA_ID, TRUE);

        if (pSchema != NULL)
        {
            g_settings_schema_unref(pSchema);
            m_gsettings_cal_notification = g_settings_new_with_path(SETTINGS_NOTIFY_SCHEMA_ID, SETTINGS_NOTIFY_CALENDAR_PATH);
        }
        else
        {
            m_gsettings_cal_notification = NULL;
        }

        m_live.reset(new LiveSettings);
        m_settings = std::dynamic_pointer_cast<Settings>(m_live);
    }

    void TearDown() override
    {
        g_clear_object(&m_gsettings_cal_notification);
        g_clear_object(&m_gsettings);
        m_settings.reset();
        m_live.reset();

        super::TearDown();
    }

    void TestBoolProperty(GSettings* gsettings, core::Property<bool>& property, const gchar* key)
    {
        EXPECT_EQ(g_settings_get_boolean(gsettings, key), property.get());
        g_settings_set_boolean(gsettings, key, false);
        EXPECT_FALSE(property.get());
        g_settings_set_boolean(gsettings, key, true);
        EXPECT_TRUE(property.get());

        property.set(false);
        EXPECT_FALSE(g_settings_get_boolean(gsettings, key));
        property.set(true);
        EXPECT_TRUE(g_settings_get_boolean(gsettings, key));
    }

    void TestStringProperty(GSettings* gsettings, core::Property<std::string>& property, const gchar* key)
    {
        gchar* tmp;
        std::string str;

        tmp = g_settings_get_string(gsettings, key);
        EXPECT_EQ(tmp, property.get());
        g_clear_pointer(&tmp, g_free);

        str = "a";
        g_settings_set_string(gsettings, key, str.c_str());
        EXPECT_EQ(str, property.get());

        str = "b";
        g_settings_set_string(gsettings, key, str.c_str());
        EXPECT_EQ(str, property.get());

        str = "a";
        property.set(str);
        tmp = g_settings_get_string(gsettings, key);
        EXPECT_EQ(str, tmp);
        g_clear_pointer(&tmp, g_free);

        str = "b";
        property.set(str);
        tmp = g_settings_get_string(gsettings, key);
        EXPECT_EQ(str, tmp);
        g_clear_pointer(&tmp, g_free);
    }

    void TestUIntProperty(GSettings* gsettings, core::Property<unsigned int>& property, const gchar* key)
    {
        EXPECT_EQ(g_settings_get_uint(gsettings, key), property.get());

        unsigned int expected_values[] = { 1, 2, 3 };

        // modify GSettings and confirm that the new value is propagated
        for(const auto& expected_value : expected_values)
        {
            g_settings_set_uint(gsettings, key, expected_value);
            EXPECT_EQ(expected_value, property.get());
            EXPECT_EQ(expected_value, g_settings_get_uint(gsettings, key));
        }

        // modify the property and confirm that the new value is propagated
        for(const auto& expected_value : expected_values)
        {
            property.set(expected_value);
            EXPECT_EQ(expected_value, property.get());
            EXPECT_EQ(expected_value, g_settings_get_uint(gsettings, key));
        }
    }
};

/***
****
***/

TEST_F(SettingsFixture, HelloWorld)
{
    EXPECT_TRUE(true);
}

TEST_F(SettingsFixture, BoolProperties)
{
    TestBoolProperty(m_gsettings, m_settings->show_seconds, SETTINGS_SHOW_SECONDS_S);
    TestBoolProperty(m_gsettings, m_settings->show_calendar, SETTINGS_SHOW_CALENDAR_S);
    TestBoolProperty(m_gsettings, m_settings->show_date, SETTINGS_SHOW_DATE_S);
    TestBoolProperty(m_gsettings, m_settings->show_day, SETTINGS_SHOW_DAY_S);
    TestBoolProperty(m_gsettings, m_settings->show_detected_location, SETTINGS_SHOW_DETECTED_S);
    TestBoolProperty(m_gsettings, m_settings->show_events, SETTINGS_SHOW_EVENTS_S);
    TestBoolProperty(m_gsettings, m_settings->show_locations, SETTINGS_SHOW_LOCATIONS_S);
    TestBoolProperty(m_gsettings, m_settings->show_week_numbers, SETTINGS_SHOW_WEEK_NUMBERS_S);
    TestBoolProperty(m_gsettings, m_settings->show_year, SETTINGS_SHOW_YEAR_S);
}

TEST_F(SettingsFixture, UIntProperties)
{
    TestUIntProperty(m_gsettings, m_settings->alarm_duration, SETTINGS_ALARM_DURATION_S);
    TestUIntProperty(m_gsettings, m_settings->alarm_volume, SETTINGS_ALARM_VOLUME_S);
    TestUIntProperty(m_gsettings, m_settings->snooze_duration, SETTINGS_SNOOZE_DURATION_S);
}

TEST_F(SettingsFixture, StringProperties)
{
    TestStringProperty(m_gsettings, m_settings->custom_time_format, SETTINGS_CUSTOM_TIME_FORMAT_S);
    TestStringProperty(m_gsettings, m_settings->timezone_name, SETTINGS_TIMEZONE_NAME_S);
    TestStringProperty(m_gsettings, m_settings->alarm_sound, SETTINGS_ALARM_SOUND_S);
    TestStringProperty(m_gsettings, m_settings->calendar_sound, SETTINGS_CALENDAR_SOUND_S);
    TestStringProperty(m_gsettings, m_settings->alarm_haptic, SETTINGS_ALARM_HAPTIC_S);
}

TEST_F(SettingsFixture, TimeFormatMode)
{
    const auto key = SETTINGS_TIME_FORMAT_S;
    const TimeFormatMode modes[] = { TIME_FORMAT_MODE_LOCALE_DEFAULT,
                                     TIME_FORMAT_MODE_12_HOUR,
                                     TIME_FORMAT_MODE_24_HOUR,
                                     TIME_FORMAT_MODE_CUSTOM };

    for(const auto& mode : modes)
    {
        g_settings_set_enum(m_gsettings, key, mode);
        EXPECT_EQ(mode, m_settings->time_format_mode.get());
    }

    for(const auto& mode : modes)
    {
        m_settings->time_format_mode.set(mode);
        EXPECT_EQ(mode, g_settings_get_enum(m_gsettings, key));
    }
}

namespace
{
    std::vector<std::string> strv_to_vector(const gchar** strv)
    {
        std::vector<std::string> v;
        for(int i=0; strv && strv[i]; i++)
            v.push_back(strv[i]);
        return v;
    }
};

TEST_F(SettingsFixture, Locations)
{
    const auto key = SETTINGS_LOCATIONS_S;

    const gchar* astrv[] = {"America/Los_Angeles Oakland", "America/Chicago Oklahoma City", "Europe/London London", nullptr};
    const gchar* bstrv[] = {"America/Denver", "Europe/London London", "Europe/Berlin Berlin", nullptr};
    const std::vector<std::string> av = strv_to_vector(astrv);
    const std::vector<std::string> bv = strv_to_vector(bstrv);

    g_settings_set_strv(m_gsettings, key, astrv);
    EXPECT_EQ(av, m_settings->locations.get());
    g_settings_set_strv(m_gsettings, key, bstrv);
    EXPECT_EQ(bv, m_settings->locations.get());

    m_settings->locations.set(av);
    auto tmp = g_settings_get_strv(m_gsettings, key);
    auto vtmp = strv_to_vector((const gchar**)tmp);
    g_strfreev(tmp);
    EXPECT_EQ(av, vtmp);

    m_settings->locations.set(bv);
    tmp = g_settings_get_strv(m_gsettings, key);
    vtmp = strv_to_vector((const gchar**)tmp);
    g_strfreev(tmp);
    EXPECT_EQ(bv, vtmp);
}

TEST_F(SettingsFixture, MutedApps)
{
    if (!m_gsettings_cal_notification) {
        return;
    }

    TestBoolProperty(m_gsettings_cal_notification, m_settings->cal_notification_enabled, SETTINGS_NOTIFY_ENABLED_KEY);
    TestBoolProperty(m_gsettings_cal_notification, m_settings->cal_notification_sounds, SETTINGS_NOTIFY_SOUNDS_KEY);
    TestBoolProperty(m_gsettings_cal_notification, m_settings->cal_notification_vibrations, SETTINGS_NOTIFY_VIBRATIONS_KEY);
    TestBoolProperty(m_gsettings_cal_notification, m_settings->cal_notification_bubbles, SETTINGS_NOTIFY_BUBBLES_KEY);
    TestBoolProperty(m_gsettings_cal_notification, m_settings->cal_notification_list, SETTINGS_NOTIFY_LIST_KEY);
}
