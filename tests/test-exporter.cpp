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

#include "actions-mock.h"
#include "state-mock.h"
#include "glib-fixture.h"

#include "dbus-alarm-properties.h"

#include <datetime/actions.h>
#include <datetime/dbus-shared.h>
#include <datetime/exporter.h>

#include <set>
#include <string>

using namespace unity::indicator::datetime;

class ExporterFixture: public GlibFixture
{
private:

    typedef GlibFixture super;

protected:

    GTestDBus* bus = nullptr;

    void SetUp() override
    {
        super::SetUp();

        // bring up the test bus
        bus = g_test_dbus_new(G_TEST_DBUS_NONE);
        g_test_dbus_up(bus);
        const auto address = g_test_dbus_get_bus_address(bus);
        g_setenv("DBUS_SYSTEM_BUS_ADDRESS", address, true);
        g_setenv("DBUS_SESSION_BUS_ADDRESS", address, true);
    }

    void TearDown() override
    {
        GError * error = nullptr;
        GDBusConnection* connection = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error);
        if(!g_dbus_connection_is_closed(connection))
            g_dbus_connection_close_sync(connection, nullptr, &error);
        g_assert_no_error(error);
        g_clear_object(&connection);
        g_test_dbus_down(bus);
        g_clear_object(&bus);

        super::TearDown();
    }
};

TEST_F(ExporterFixture, HelloWorld)
{
    // confirms that the Test DBus SetUp() and TearDown() works
}

TEST_F(ExporterFixture, Publish)
{
    auto state = std::make_shared<MockState>();
    auto actions = std::make_shared<MockActions>(state);
    auto settings = std::make_shared<Settings>();
    std::vector<std::shared_ptr<Menu>> menus;

    MenuFactory menu_factory (actions, state);
    for(int i=0; i<Menu::NUM_PROFILES; i++)
      menus.push_back(menu_factory.buildMenu(Menu::Profile(i)));

    Exporter exporter(settings);
    exporter.publish(actions, menus);
    wait_msec();

    auto connection = g_bus_get_sync (G_BUS_TYPE_SESSION, nullptr, nullptr);
    auto exported = g_dbus_action_group_get (connection, BUS_DATETIME_NAME, BUS_DATETIME_PATH);
    auto names_strv = g_action_group_list_actions(G_ACTION_GROUP(exported));

    // wait for the exported ActionGroup to be populated
    if (g_strv_length(names_strv) == 0)
    {
        g_strfreev(names_strv);
        wait_for_signal(exported, "action-added");
        names_strv = g_action_group_list_actions(G_ACTION_GROUP(exported));
    }

    // convert it to a std::set for easy prodding
    std::set<std::string> names;
    for(int i=0; names_strv && names_strv[i]; i++)
      names.insert(names_strv[i]);

    // confirm the actions that we expect
    EXPECT_EQ(1, names.count("desktop.open-alarm-app"));
    EXPECT_EQ(1, names.count("desktop.open-appointment"));
    EXPECT_EQ(1, names.count("desktop.open-calendar-app"));
    EXPECT_EQ(1, names.count("desktop.open-settings-app"));
    EXPECT_EQ(1, names.count("phone.open-alarm-app"));
    EXPECT_EQ(1, names.count("phone.open-appointment"));
    EXPECT_EQ(1, names.count("phone.open-calendar-app"));
    EXPECT_EQ(1, names.count("phone.open-settings-app"));
    EXPECT_EQ(1, names.count("calendar"));
    EXPECT_EQ(1, names.count("desktop_greeter-header"));
    EXPECT_EQ(1, names.count("desktop-header"));
    EXPECT_EQ(1, names.count("phone_greeter-header"));
    EXPECT_EQ(1, names.count("phone-header"));
    EXPECT_EQ(1, names.count("set-location"));

    // try closing the connection prematurely
    // to test Exporter's name-lost signal
    bool name_lost = false;
    exporter.name_lost().connect([this,&name_lost](){
        name_lost = true;
        g_main_loop_quit(loop);
    });
    g_dbus_connection_close_sync(connection, nullptr, nullptr);
    g_main_loop_run(loop);
    EXPECT_TRUE(name_lost);

    // cleanup
    g_strfreev(names_strv);
    g_clear_object(&exported);
    g_clear_object(&connection);
}

TEST_F(ExporterFixture, AlarmProperties)
{
    /***
    **** Set up the exporter
    ***/

    std::shared_ptr<State> state(new MockState);
    std::shared_ptr<Actions> actions(new MockActions(state));
    std::shared_ptr<Settings> settings(new Settings);
    std::vector<std::shared_ptr<Menu>> menus;

    MenuFactory menu_factory (actions, state);
    for(int i=0; i<Menu::NUM_PROFILES; i++)
      menus.push_back(menu_factory.buildMenu(Menu::Profile(i)));

    Exporter exporter(settings);
    exporter.publish(actions, menus);
    wait_msec();

    /***
    **** Set up the proxy
    ***/

    auto on_proxy_ready = [](GObject*, GAsyncResult* res, gpointer gproxy){
        GError* error = nullptr;
        *reinterpret_cast<DatetimeAlarmProperties**>(gproxy) = datetime_alarm_properties_proxy_new_for_bus_finish(res, &error);
        EXPECT_TRUE(error == nullptr);
    };

    DatetimeAlarmProperties* proxy = nullptr;
    datetime_alarm_properties_proxy_new_for_bus(G_BUS_TYPE_SESSION,
                                                G_DBUS_PROXY_FLAGS_NONE,
                                                BUS_DATETIME_NAME,
                                                BUS_DATETIME_PATH"/AlarmProperties",
                                                nullptr,
                                                on_proxy_ready,
                                                &proxy);
    wait_msec(100);
    ASSERT_TRUE(proxy != nullptr);

    /***
    **** Try changing the Settings -- do the DBus properties change to match it?
    ***/

    auto expected_volume = 1;
    int expected_duration = 4;
    int expected_snooze_duration = 5;
    const char * expected_sound = "/tmp/foo.wav";
    const char * expected_haptic = "pulse";
    settings->alarm_volume.set(expected_volume);
    settings->alarm_duration.set(expected_duration);
    settings->snooze_duration.set(expected_snooze_duration);
    settings->alarm_sound.set(expected_sound);
    settings->alarm_haptic.set(expected_haptic);
    wait_msec();

    static constexpr const char* const SOUND_PROP {"default-sound"};
    static constexpr const char* const VOLUME_PROP {"default-volume"};
    static constexpr const char* const DURATION_PROP {"duration"};
    static constexpr const char* const HAPTIC_PROP {"haptic-feedback"};
    static constexpr const char* const SNOOZE_PROP {"snooze-duration"};

    char* sound = nullptr;
    char* haptic = nullptr;
    int volume = -1;
    int duration = -1;
    int snooze = -1;
    g_object_get(proxy, SOUND_PROP, &sound,
                        HAPTIC_PROP, &haptic,
                        VOLUME_PROP, &volume,
                        DURATION_PROP, &duration,
                        SNOOZE_PROP, &snooze,
                        nullptr);
    EXPECT_STREQ(expected_sound, sound);
    EXPECT_STREQ(expected_haptic, haptic);
    EXPECT_EQ(expected_volume, volume);
    EXPECT_EQ(expected_duration, duration);
    EXPECT_EQ(expected_snooze_duration, snooze);

    g_clear_pointer (&sound, g_free);
    g_clear_pointer (&haptic, g_free);

    /***
    **** Try changing the DBus properties -- do the Settings change to match it?
    ***/

    expected_volume = 100;
    expected_duration = 30;
    expected_sound = "/tmp/bar.wav";
    expected_haptic = "none";
    expected_snooze_duration = 5;
    g_object_set(proxy, SOUND_PROP, expected_sound,
                        HAPTIC_PROP, expected_haptic,
                        VOLUME_PROP, expected_volume,
                        DURATION_PROP, expected_duration,
                        SNOOZE_PROP, expected_snooze_duration,
                        nullptr);
    wait_msec();

    EXPECT_STREQ(expected_sound, settings->alarm_sound.get().c_str());
    EXPECT_STREQ(expected_haptic, settings->alarm_haptic.get().c_str());
    EXPECT_EQ(expected_volume, settings->alarm_volume.get());
    EXPECT_EQ(expected_duration, settings->alarm_duration.get());
    EXPECT_EQ(expected_snooze_duration, settings->snooze_duration.get());

    // cleanup
    g_clear_object(&proxy);
}
