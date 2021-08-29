/*
 * Copyright 2014-2016 Canonical Ltd.
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

#pragma once

#include "libdbusmock-fixture.h"

#include <datetime/appointment.h>
#include <datetime/dbus-shared.h>
#include <datetime/settings.h>
#include <datetime/snap.h>

#include <notifications/dbus-shared.h>
#include <notifications/notifications.h>

#include <libdbustest/dbus-test.h>

#include <unistd.h> // getuid()
#include <sys/types.h> // getuid()

/***
****
***/

class NotificationFixture: public LibdbusmockFixture
{
private:

  typedef LibdbusmockFixture super;

protected:

  static constexpr char const * NOTIFY_BUSNAME   {"org.freedesktop.Notifications"};
  static constexpr char const * NOTIFY_INTERFACE {"org.freedesktop.Notifications"};
  static constexpr char const * NOTIFY_PATH      {"/org/freedesktop/Notifications"};

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
  static constexpr char const * AS_INTERFACE          {"com.lomiri.touch.AccountsService.Sound"};
  static constexpr char const * PROP_OTHER_VIBRATIONS {"OtherVibrate"};
  static constexpr char const * PROP_SILENT_MODE      {"SilentMode"};

  ayatana::indicator::datetime::Appointment appt;
  ayatana::indicator::datetime::Appointment ualarm;

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
    appt.type = ayatana::indicator::datetime::Appointment::EVENT;
    const auto christmas = ayatana::indicator::datetime::DateTime::Local(2015,12,25,0,0,0);
    appt.begin = christmas.start_of_day();
    appt.end = christmas.end_of_day();
    appt.alarms.push_back(ayatana::indicator::datetime::Alarm{"Ho Ho Ho!", CALENDAR_DEFAULT_SOUND, appt.begin});

    // init a Lomiri Alarm
    ualarm.color = "red";
    ualarm.summary = "Wakeup";
    ualarm.uid = "E4B57D50247291478ED31DED17FF0A9838DED403";
    ualarm.type = ayatana::indicator::datetime::Appointment::UBUNTU_ALARM;
    const auto tomorrow = ayatana::indicator::datetime::DateTime::NowLocal().add_days(1);
    ualarm.begin = tomorrow;
    ualarm.end = tomorrow;
    ualarm.alarms.push_back(ayatana::indicator::datetime::Alarm{"It's Tomorrow!", "", appt.begin});

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

    startDbusMock();
  }

  void TearDown() override
  {
    g_clear_object(&haptic_mock);
    g_clear_object(&screen_mock);
    g_clear_object(&powerd_mock);
    g_clear_object(&notify_mock);
    g_clear_object(&as_mock);

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

  std::shared_ptr<ayatana::indicator::datetime::Snap>
  create_snap(const std::shared_ptr<ayatana::indicator::notifications::Engine>& ne,
              const std::shared_ptr<ayatana::indicator::notifications::SoundBuilder>& sb,
              const std::shared_ptr<ayatana::indicator::datetime::Settings>& settings)
  {
    auto snap = std::make_shared<ayatana::indicator::datetime::Snap>(ne, sb, settings, system_bus);
    wait_msec(100); // wait a moment for the Snap to finish its async dbus bootstrapping
    return snap;
  }
};

