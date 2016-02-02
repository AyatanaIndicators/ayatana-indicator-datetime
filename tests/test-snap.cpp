/*
 * Copyright 2014 Canonical Ltd.
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

#include <datetime/appointment.h>
#include <datetime/dbus-shared.h>
#include <datetime/settings.h>
#include <datetime/snap.h>

#include <notifications/dbus-shared.h>
#include <notifications/notifications.h>

#include <libdbustest/dbus-test.h>

#include <glib.h>

#include <unistd.h> // getuid()
#include <sys/types.h> // getuid()

using namespace ayatana::indicator::datetime;

namespace uin = ayatana::indicator::notifications;

#include "glib-fixture.h"

/***
****
***/

namespace
{
  static constexpr char const * APP_NAME {"ayatana-indicator-datetime-service"};
}

using namespace ayatana::indicator::datetime;

class SnapFixture: public GlibFixture
{
private:

  typedef GlibFixture super;

  static constexpr char const * NOTIFY_BUSNAME   {"org.freedesktop.Notifications"};
  static constexpr char const * NOTIFY_INTERFACE {"org.freedesktop.Notifications"};
  static constexpr char const * NOTIFY_PATH      {"/org/freedesktop/Notifications"};

protected:

  static constexpr char const * HAPTIC_METHOD_VIBRATE_PATTERN {"VibratePattern"};

  static constexpr int SCREEN_COOKIE {8675309};
  static constexpr char const * SCREEN_METHOD_KEEP_DISPLAY_ON {"keepDisplayOn"};
  static constexpr char const * SCREEN_METHOD_REMOVE_DISPLAY_ON_REQUEST {"removeDisplayOnRequest"};

  static constexpr int POWERD_SYS_STATE_ACTIVE = 1;
  static constexpr char const * POWERD_COOKIE {"567-48-8307"};
  static constexpr char const * POWERD_METHOD_REQUEST_SYS_STATE {"requestSysState"};
  static constexpr char const * POWERD_METHOD_CLEAR_SYS_STATE {"clearSysState"};

  static constexpr int FIRST_NOTIFY_ID {1000};

  static constexpr int NOTIFICATION_CLOSED_EXPIRED   {1};
  static constexpr int NOTIFICATION_CLOSED_DISMISSED {2};
  static constexpr int NOTIFICATION_CLOSED_API       {3};
  static constexpr int NOTIFICATION_CLOSED_UNDEFINED {4};

  static constexpr char const * METHOD_CLOSE {"CloseNotification"};
  static constexpr char const * METHOD_GET_CAPS {"GetCapabilities"};
  static constexpr char const * METHOD_GET_INFO {"GetServerInformation"};
  static constexpr char const * METHOD_NOTIFY {"Notify"};

  static constexpr char const * SIGNAL_CLOSED {"NotificationClosed"};

  static constexpr char const * HINT_TIMEOUT {"x-canonical-snap-decisions-timeout"};

  static constexpr char const * AS_BUSNAME            {"org.freedesktop.Accounts"};
  static constexpr char const * AS_INTERFACE          {"com.ubuntu.touch.AccountsService.Sound"};
  static constexpr char const * PROP_OTHER_VIBRATIONS {"OtherVibrate"};
  static constexpr char const * PROP_SILENT_MODE      {"SilentMode"};

  Appointment appt;
  Appointment ualarm;
  GDBusConnection * system_bus = nullptr;
  GDBusConnection * session_bus = nullptr;
  DbusTestService * service = nullptr;
  DbusTestDbusMock * as_mock = nullptr;
  DbusTestDbusMock * notify_mock = nullptr;
  DbusTestDbusMock * powerd_mock = nullptr;
  DbusTestDbusMock * screen_mock = nullptr;
  DbusTestDbusMock * haptic_mock = nullptr;
  DbusTestDbusMockObject * as_obj = nullptr;
  DbusTestDbusMockObject * notify_obj = nullptr;
  DbusTestDbusMockObject * powerd_obj = nullptr;
  DbusTestDbusMockObject * screen_obj = nullptr;
  DbusTestDbusMockObject * haptic_obj = nullptr;

  void SetUp() override
  {
    GError * error = nullptr;
    char * str = nullptr;

    super::SetUp();

    // init an Appointment
    appt.color = "green";
    appt.summary = "Christmas";
    appt.uid = "D4B57D50247291478ED31DED17FF0A9838DED402";
    appt.type = Appointment::EVENT;
    const auto christmas = DateTime::Local(2015,12,25,0,0,0);
    appt.begin = christmas.start_of_day();
    appt.end = christmas.end_of_day();
    appt.alarms.push_back(Alarm{"Ho Ho Ho!", "", appt.begin});

    // init an Ubuntu Alarm
    ualarm.color = "red";
    ualarm.summary = "Wakeup";
    ualarm.uid = "E4B57D50247291478ED31DED17FF0A9838DED403";
    ualarm.type = Appointment::UBUNTU_ALARM;
    const auto tomorrow = DateTime::NowLocal().add_days(1);
    ualarm.begin = tomorrow;
    ualarm.end = tomorrow;
    ualarm.alarms.push_back(Alarm{"It's Tomorrow!", "", appt.begin});

    service = dbus_test_service_new(nullptr);

    ///
    ///  Add the AccountsService mock
    ///

    as_mock = dbus_test_dbus_mock_new(AS_BUSNAME);
    auto as_path = g_strdup_printf("/org/freedesktop/Accounts/User%lu", (gulong)getuid());
    as_obj = dbus_test_dbus_mock_get_object(as_mock,
                                            as_path,
                                            AS_INTERFACE,
                                            &error);
    g_free(as_path);
    g_assert_no_error(error);

    // PROP_SILENT_MODE
    dbus_test_dbus_mock_object_add_property(as_mock,
                                            as_obj,
                                            PROP_SILENT_MODE,
                                            G_VARIANT_TYPE_BOOLEAN,
                                            g_variant_new_boolean(false),
                                            &error);
    g_assert_no_error(error);

    // PROP_OTHER_VIBRATIONS
    dbus_test_dbus_mock_object_add_property(as_mock,
                                            as_obj,
                                            PROP_OTHER_VIBRATIONS,
                                            G_VARIANT_TYPE_BOOLEAN,
                                            g_variant_new_boolean(true),
                                            &error);
    g_assert_no_error(error);
    dbus_test_service_add_task(service, DBUS_TEST_TASK(as_mock));

    ///
    ///  Add the Notifications mock
    ///

    notify_mock = dbus_test_dbus_mock_new(NOTIFY_BUSNAME);
    notify_obj = dbus_test_dbus_mock_get_object(notify_mock,
                                                NOTIFY_PATH,
                                                NOTIFY_INTERFACE,
                                                &error);
    g_assert_no_error(error);

    // METHOD_GET_INFO
    str = g_strdup("ret = ('mock-notify', 'test vendor', '1.0', '1.1')");
    dbus_test_dbus_mock_object_add_method(notify_mock,
                                          notify_obj,
                                          METHOD_GET_INFO,
                                          nullptr,
                                          G_VARIANT_TYPE("(ssss)"),
                                          str,
                                          &error);
    g_assert_no_error (error);
    g_free (str);

    // METHOD_NOTIFY
    str = g_strdup_printf("try:\n"
                          "  self.NextNotifyId\n"
                          "except AttributeError:\n"
                          "  self.NextNotifyId = %d\n"
                          "ret = self.NextNotifyId\n"
                          "self.NextNotifyId += 1\n",
                          FIRST_NOTIFY_ID);
    dbus_test_dbus_mock_object_add_method(notify_mock,
                                          notify_obj,
                                          METHOD_NOTIFY,
                                          G_VARIANT_TYPE("(susssasa{sv}i)"),
                                          G_VARIANT_TYPE_UINT32,
                                          str,
                                          &error);
    g_assert_no_error (error);
    g_free (str);

    // METHOD_CLOSE
    str = g_strdup_printf("self.EmitSignal('%s', '%s', 'uu', [ args[0], %d ])",
                          NOTIFY_INTERFACE,
                          SIGNAL_CLOSED,
                          NOTIFICATION_CLOSED_API);
    dbus_test_dbus_mock_object_add_method(notify_mock,
                                          notify_obj,
                                          METHOD_CLOSE,
                                          G_VARIANT_TYPE("(u)"),
                                          nullptr,
                                          str,
                                          &error);
    g_assert_no_error (error);
    g_free (str);

    dbus_test_service_add_task(service, DBUS_TEST_TASK(notify_mock));

    ///
    ///  Add the powerd mock
    ///

    powerd_mock = dbus_test_dbus_mock_new(BUS_POWERD_NAME);
    powerd_obj = dbus_test_dbus_mock_get_object(powerd_mock,
                                                BUS_POWERD_PATH,
                                                BUS_POWERD_INTERFACE,
                                                &error);
    g_assert_no_error(error);

    str = g_strdup_printf ("ret = '%s'", POWERD_COOKIE);
    dbus_test_dbus_mock_object_add_method(powerd_mock,
                                          powerd_obj,
                                          POWERD_METHOD_REQUEST_SYS_STATE,
                                          G_VARIANT_TYPE("(si)"),
                                          G_VARIANT_TYPE("(s)"),
                                          str,
                                          &error);
    g_assert_no_error (error);
    g_free (str);

    dbus_test_dbus_mock_object_add_method(powerd_mock,
                                          powerd_obj,
                                          POWERD_METHOD_CLEAR_SYS_STATE,
                                          G_VARIANT_TYPE("(s)"),
                                          nullptr,
                                          "",
                                          &error);
    g_assert_no_error (error);

    dbus_test_service_add_task(service, DBUS_TEST_TASK(powerd_mock));

    ///
    ///  Add the Screen mock
    ///

    screen_mock = dbus_test_dbus_mock_new(BUS_SCREEN_NAME);
    screen_obj = dbus_test_dbus_mock_get_object(screen_mock,
                                                BUS_SCREEN_PATH,
                                                BUS_SCREEN_INTERFACE,
                                                &error);
    g_assert_no_error(error);

    str = g_strdup_printf ("ret = %d", SCREEN_COOKIE);
    dbus_test_dbus_mock_object_add_method(screen_mock,
                                          screen_obj,
                                          SCREEN_METHOD_KEEP_DISPLAY_ON,
                                          nullptr,
                                          G_VARIANT_TYPE("(i)"),
                                          str,
                                          &error);
    g_assert_no_error (error);
    g_free (str);

    dbus_test_dbus_mock_object_add_method(screen_mock,
                                          screen_obj,
                                          SCREEN_METHOD_REMOVE_DISPLAY_ON_REQUEST,
                                          G_VARIANT_TYPE("(i)"),
                                          nullptr,
                                          "",
                                          &error);
    g_assert_no_error (error);
    dbus_test_service_add_task(service, DBUS_TEST_TASK(screen_mock));

    ///
    ///  Add the haptic mock
    ///

    haptic_mock = dbus_test_dbus_mock_new(BUS_HAPTIC_NAME);
    haptic_obj = dbus_test_dbus_mock_get_object(haptic_mock,
                                                BUS_HAPTIC_PATH,
                                                BUS_HAPTIC_INTERFACE,
                                                &error);

    dbus_test_dbus_mock_object_add_method(haptic_mock,
                                          haptic_obj,
                                          HAPTIC_METHOD_VIBRATE_PATTERN,
                                          G_VARIANT_TYPE("(auu)"),
                                          nullptr,
                                          "",
                                          &error);
    g_assert_no_error (error);
    dbus_test_service_add_task(service, DBUS_TEST_TASK(haptic_mock));


    // start 'em up.
    // make the system bus work off the mock bus too, since that's
    // where the upower and screen are on the system bus...

    dbus_test_service_start_tasks(service);
    g_setenv("DBUS_SYSTEM_BUS_ADDRESS", g_getenv("DBUS_SESSION_BUS_ADDRESS"), TRUE);

    session_bus = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, nullptr);
    ASSERT_NE(nullptr, session_bus);
    g_dbus_connection_set_exit_on_close(session_bus, false);
    g_object_add_weak_pointer(G_OBJECT(session_bus), (gpointer *)&session_bus);

    system_bus = g_bus_get_sync(G_BUS_TYPE_SYSTEM, nullptr, nullptr);
    ASSERT_NE(nullptr, system_bus);
    g_dbus_connection_set_exit_on_close(system_bus, FALSE);
    g_object_add_weak_pointer(G_OBJECT(system_bus), (gpointer *)&system_bus);
  }

  void TearDown() override
  {
    g_clear_object(&haptic_mock);
    g_clear_object(&screen_mock);
    g_clear_object(&powerd_mock);
    g_clear_object(&notify_mock);
    g_clear_object(&as_mock);
    g_clear_object(&service);
    g_object_unref(session_bus);
    g_object_unref(system_bus);

    // wait a little while for the scaffolding to shut down,
    // but don't block on it forever...
    unsigned int cleartry = 0;
    while (((system_bus != nullptr) || (session_bus != nullptr)) && (cleartry < 50))
      {
        g_usleep(100000);
        while (g_main_pending())
          g_main_iteration(true);
        cleartry++;
      }

    super::TearDown();
  }

  void make_interactive()
  {
    // GetCapabilities returns an array containing 'actions',
    // so our snap decision will be interactive.
    // For this test, it means we should get a timeout Notify Hint
    // that matches duration_minutes
    GError * error = nullptr;
    dbus_test_dbus_mock_object_add_method(notify_mock,
                                          notify_obj,
                                          METHOD_GET_CAPS,
                                          nullptr,
                                          G_VARIANT_TYPE_STRING_ARRAY,
                                          "ret = ['actions', 'body']",
                                          &error);
    g_assert_no_error (error);
  }

  std::shared_ptr<Snap> create_snap(const std::shared_ptr<uin::Engine>& ne,
                                    const std::shared_ptr<uin::SoundBuilder>& sb,
                                    const std::shared_ptr<Settings>& settings)
  {
    auto snap = std::make_shared<Snap>(ne, sb, settings);
    wait_msec(100); // wait a moment for the Snap to finish its async dbus bootstrapping
    return snap;
  }
};

/***
****
***/

namespace
{
  gboolean quit_idle (gpointer gloop)
  {
    g_main_loop_quit(static_cast<GMainLoop*>(gloop));
    return G_SOURCE_REMOVE;
  };
}

TEST_F(SnapFixture, InteractiveDuration)
{
  static constexpr int duration_minutes = 120;
  auto settings = std::make_shared<Settings>();
  settings->alarm_duration.set(duration_minutes);
  auto ne = std::make_shared<ayatana::indicator::notifications::Engine>(APP_NAME);
  auto sb = std::make_shared<ayatana::indicator::notifications::DefaultSoundBuilder>();
  auto snap = create_snap(ne, sb, settings);

  make_interactive();

  // call the Snap Decision
  auto func = [this](const Appointment&, const Alarm&){g_idle_add(quit_idle, loop);};
  (*snap)(appt, appt.alarms.front(), func, func);

  // confirm that Notify got called once
  guint len = 0;
  GError * error = nullptr;
  const auto calls = dbus_test_dbus_mock_object_get_method_calls (notify_mock,
                                                                  notify_obj,
                                                                  METHOD_NOTIFY,
                                                                  &len,
                                                                  &error);
  g_assert_no_error(error);
  ASSERT_EQ(1, len);

  // confirm that the app_name passed to Notify was APP_NAME
  const auto& params = calls[0].params;
  ASSERT_EQ(8, g_variant_n_children(params));
  const char * str = nullptr;
  g_variant_get_child (params, 0, "&s", &str);
  ASSERT_STREQ(APP_NAME, str);

  // confirm that the icon passed to Notify was "alarm-clock"
  g_variant_get_child (params, 2, "&s", &str);
  ASSERT_STREQ("alarm-clock", str);

  // confirm that the hints passed to Notify included a timeout matching duration_minutes
  int32_t i32;
  bool b;
  auto hints = g_variant_get_child_value (params, 6);
  b = g_variant_lookup (hints, HINT_TIMEOUT, "i", &i32);
  EXPECT_TRUE(b);
  const auto duration = std::chrono::minutes(duration_minutes);
  EXPECT_EQ(std::chrono::duration_cast<std::chrono::milliseconds>(duration).count(), i32);
  g_variant_unref(hints);
  ne.reset();
}

/***
****
***/

TEST_F(SnapFixture, InhibitSleep)
{
  auto settings = std::make_shared<Settings>();
  auto ne = std::make_shared<ayatana::indicator::notifications::Engine>(APP_NAME);
  auto sb = std::make_shared<ayatana::indicator::notifications::DefaultSoundBuilder>();
  auto snap = create_snap(ne, sb, settings);

  make_interactive();

  // invoke the notification
  auto func = [this](const Appointment&, const Alarm&){g_idle_add(quit_idle, loop);};
  (*snap)(appt, appt.alarms.front(), func, func);

  wait_msec(1000);

  // confirm that sleep got inhibited
  GError * error = nullptr;
  EXPECT_TRUE (dbus_test_dbus_mock_object_check_method_call (powerd_mock,
                                                             powerd_obj,
                                                             POWERD_METHOD_REQUEST_SYS_STATE,
                                                             g_variant_new("(si)", APP_NAME, POWERD_SYS_STATE_ACTIVE),
                                                             &error));

  // confirm that the screen got forced on
  EXPECT_TRUE (dbus_test_dbus_mock_object_check_method_call (screen_mock,
                                                             screen_obj,
                                                             SCREEN_METHOD_KEEP_DISPLAY_ON,
                                                             nullptr,
                                                             &error));

  // force-close the snap
  wait_msec(100);
  snap.reset();
  wait_msec(100);

  // confirm that sleep got uninhibted
  EXPECT_TRUE (dbus_test_dbus_mock_object_check_method_call (powerd_mock,
                                                             powerd_obj,
                                                             POWERD_METHOD_CLEAR_SYS_STATE,
                                                             g_variant_new("(s)", POWERD_COOKIE),
                                                             &error));

  // confirm that the screen's no longer forced on
  EXPECT_TRUE (dbus_test_dbus_mock_object_check_method_call (screen_mock,
                                                             screen_obj,
                                                             SCREEN_METHOD_REMOVE_DISPLAY_ON_REQUEST,
                                                             g_variant_new("(i)", SCREEN_COOKIE),
                                                             &error));

  g_assert_no_error (error);
}

/***
****
***/

TEST_F(SnapFixture, ForceScreen)
{
  auto settings = std::make_shared<Settings>();
  auto ne = std::make_shared<ayatana::indicator::notifications::Engine>(APP_NAME);
  auto sb = std::make_shared<ayatana::indicator::notifications::DefaultSoundBuilder>();
  auto snap = create_snap(ne, sb, settings);

  make_interactive();

  // invoke the notification
  auto func = [this](const Appointment&, const Alarm&){g_idle_add(quit_idle, loop);};
  (*snap)(appt, appt.alarms.front(), func, func);

  wait_msec(1000);

  // confirm that sleep got inhibited
  GError * error = nullptr;
  EXPECT_TRUE (dbus_test_dbus_mock_object_check_method_call (powerd_mock,
                                                             powerd_obj,
                                                             POWERD_METHOD_REQUEST_SYS_STATE,
                                                             g_variant_new("(si)", APP_NAME, POWERD_SYS_STATE_ACTIVE),
                                                             &error));
  g_assert_no_error(error);

  // force-close the snap
  wait_msec(100);
  snap.reset();
  wait_msec(100);

  // confirm that sleep got uninhibted
  EXPECT_TRUE (dbus_test_dbus_mock_object_check_method_call (powerd_mock,
                                                             powerd_obj,
                                                             POWERD_METHOD_CLEAR_SYS_STATE,
                                                             g_variant_new("(s)", POWERD_COOKIE),
                                                             &error));
  g_assert_no_error(error);
}

/***
****
***/

TEST_F(SnapFixture,Notification)
{
  auto settings = std::make_shared<Settings>();
  auto ne = std::make_shared<ayatana::indicator::notifications::Engine>(APP_NAME);
  auto sb = std::make_shared<ayatana::indicator::notifications::DefaultSoundBuilder>();
  auto func = [this](const Appointment&, const Alarm&){g_idle_add(quit_idle, loop);};

  struct {
    Appointment appt;
    bool expected_notify_called;
    bool expected_vibrate_called;
  } test_appts[] = {
    { appt, true, true },
    { ualarm, true, true }
  };

  struct {
    const char* haptic_mode;
    bool expected_notify_called;
    bool expected_vibrate_called;
  } test_haptics[] = {
    { "none", true, false },
    { "pulse", true, true }
  };

  struct {
    bool other_vibrations; // the com.lomiri.touch.AccountsService.Sound "other vibrations" setting
    bool expected_notify_called;
    bool expected_vibrate_called;
  } test_other_vibrations[] = {
    { true, true, true },
    { false, true, false }
  };

  const std::set<std::pair<std::string,std::string>> blacklist_calendar { std::make_pair(std::string{"com.lomiri.calendar"}, std::string{"calendar-app"}) };
  const std::set<std::pair<std::string,std::string>> blacklist_empty;
  struct {
    std::set<std::pair<std::string,std::string>> muted_apps; // apps that should not trigger notifications
    bool expected_notify_called; // do we expect the notification tho show?
    bool expected_vibrate_called; // do we expect the phone to vibrate?
  } test_muted_apps[] = {
    { blacklist_calendar, false, false },
    { blacklist_empty, true, true }
  };

  for (const auto& test_appt : test_appts)
  {
  for (const auto& test_haptic : test_haptics)
  {
  for (const auto& test_vibes : test_other_vibrations)
  {
  for (const auto& test_muted : test_muted_apps)
  {
  auto snap = create_snap(ne, sb, settings);

    const bool expected_notify_called = test_appt.expected_notify_called
                                     && test_vibes.expected_notify_called
                                     && test_muted.expected_notify_called
                                     && test_haptic.expected_notify_called;

    const bool expected_vibrate_called = test_appt.expected_vibrate_called
                                      && test_vibes.expected_vibrate_called
                                      && test_muted.expected_vibrate_called
                                      && test_haptic.expected_vibrate_called;

g_message("appt:%s", test_appt.appt.summary.c_str());
g_message("haptic_mode:%s", test_haptic.haptic_mode);
g_message("other_vibrations:%d", (int)test_vibes.other_vibrations);
g_message("muted_apps.size():%d", (int)test_muted.muted_apps.size());
g_message("expected_notify_called %d (%d %d %d %d)", (int)expected_notify_called, (int)test_appt.expected_notify_called, (int)test_vibes.expected_notify_called, (int)test_muted.expected_notify_called, (int)test_haptic.expected_notify_called);
g_message("expected_vibrate_called %d (%d %d %d %d)", (int)expected_vibrate_called, (int)test_appt.expected_vibrate_called, (int)test_vibes.expected_vibrate_called, (int)test_muted.expected_vibrate_called, (int)test_haptic.expected_vibrate_called);

    // clear out any previous iterations' noise
    GError * error = nullptr;
    dbus_test_dbus_mock_object_clear_method_calls(haptic_mock, haptic_obj, &error);
    g_assert_no_error(error);
    dbus_test_dbus_mock_object_clear_method_calls(notify_mock, notify_obj, &error);
    g_assert_no_error(error);

    // set the properties to match the test case
    settings->muted_apps.set(test_muted.muted_apps);
    settings->alarm_haptic.set(test_haptic.haptic_mode);
    dbus_test_dbus_mock_object_update_property(as_mock,
                                               as_obj,
                                               PROP_OTHER_VIBRATIONS,
                                               g_variant_new_boolean(test_vibes.other_vibrations),
                                               &error);
    g_assert_no_error(error);
    wait_msec(200);

    // run the test
    (*snap)(appt, appt.alarms.front(), func, func);
    wait_msec(200);

    // test that the notification was as expected
    const bool notify_called = dbus_test_dbus_mock_object_check_method_call(notify_mock,
                                                                            notify_obj,
                                                                            METHOD_NOTIFY,
                                                                            nullptr,
                                                                            &error);
    g_assert_no_error(error);
    EXPECT_EQ(expected_notify_called, notify_called);

    // test that the vibration was as expected
    const bool vibrate_called = dbus_test_dbus_mock_object_check_method_call(haptic_mock,
                                                                             haptic_obj,
                                                                             HAPTIC_METHOD_VIBRATE_PATTERN,
                                                                             nullptr,
                                                                             &error);
    g_assert_no_error(error);
    EXPECT_EQ(expected_vibrate_called, vibrate_called);
  }
  }
  }
  }
}
