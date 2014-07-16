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
#include <datetime/settings.h>
#include <datetime/snap.h>
#include <datetime/timezones.h>

#include <libdbustest/dbus-test.h>

#include <libnotify/notify.h>

#include <glib.h>

using namespace unity::indicator::datetime;

#include "glib-fixture.h"

/***
****
***/

using namespace unity::indicator::datetime;

class SnapFixture: public GlibFixture
{
private:

  typedef GlibFixture super;

  static constexpr char const * NOTIFY_BUSNAME   {"org.freedesktop.Notifications"};
  static constexpr char const * NOTIFY_INTERFACE {"org.freedesktop.Notifications"};
  static constexpr char const * NOTIFY_PATH      {"/org/freedesktop/Notifications"};

protected:

  static constexpr int NOTIFY_ID {1234};

  static constexpr int NOTIFICATION_CLOSED_EXPIRED   {1};
  static constexpr int NOTIFICATION_CLOSED_DISMISSED {2};
  static constexpr int NOTIFICATION_CLOSED_API       {3};
  static constexpr int NOTIFICATION_CLOSED_UNDEFINED {4};

  static constexpr char const * APP_NAME {"indicator-datetime-service"};

  static constexpr char const * METHOD_NOTIFY {"Notify"};
  static constexpr char const * METHOD_GET_CAPS {"GetCapabilities"};
  static constexpr char const * METHOD_GET_INFO {"GetServerInformation"};

  static constexpr char const * HINT_TIMEOUT {"x-canonical-snap-decisions-timeout"};

  DbusTestService * service = nullptr;
  DbusTestDbusMock * mock = nullptr;
  DbusTestDbusMockObject * obj = nullptr;
  GDBusConnection * bus = nullptr;
  Appointment appt;

  void SetUp()
  {
    super::SetUp();

    // init the Appointment
    appt.color = "green";
    appt.summary = "Alarm";
    appt.url = "alarm:///hello-world";
    appt.uid = "D4B57D50247291478ED31DED17FF0A9838DED402";
    appt.has_alarms = true;
    auto begin = g_date_time_new_local(2014,12,25,0,0,0);
    auto end = g_date_time_add_full(begin,0,0,1,0,0,-1);
    appt.begin = begin;
    appt.end = end;
    g_date_time_unref(end);
    g_date_time_unref(begin);

    //
    // init DBusMock / dbus-test-runner
    //

    service = dbus_test_service_new(nullptr);

    GError * error = nullptr;
    mock = dbus_test_dbus_mock_new(NOTIFY_BUSNAME);
    obj = dbus_test_dbus_mock_get_object(mock, NOTIFY_PATH, NOTIFY_INTERFACE, &error);
    g_assert_no_error (error);
    
    dbus_test_dbus_mock_object_add_method(mock, obj, METHOD_GET_INFO,
                                          nullptr,
                                          G_VARIANT_TYPE("(ssss)"),
                                          "ret = ('mock-notify', 'test vendor', '1.0', '1.1')", // python
                                          &error);
    g_assert_no_error (error);

    auto python_str = g_strdup_printf ("ret = %d", NOTIFY_ID);
    dbus_test_dbus_mock_object_add_method(mock, obj, METHOD_NOTIFY,
                                          G_VARIANT_TYPE("(susssasa{sv}i)"),
                                          G_VARIANT_TYPE_UINT32,
                                          python_str,
                                          &error);
    g_free (python_str);
    g_assert_no_error (error);

    dbus_test_service_add_task(service, DBUS_TEST_TASK(mock));
    dbus_test_service_start_tasks(service);

    bus = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, nullptr);
    g_dbus_connection_set_exit_on_close(bus, FALSE);
    g_object_add_weak_pointer(G_OBJECT(bus), (gpointer *)&bus);

    notify_init(APP_NAME);
  }

  virtual void TearDown()
  {
    notify_uninit();

    g_clear_object(&mock);
    g_clear_object(&service);
    g_object_unref(bus);

    // wait a little while for the scaffolding to shut down,
    // but don't block on it forever...
    unsigned int cleartry = 0;
    while ((bus != nullptr) && (cleartry < 50))
      {
        g_usleep(100000);
        while (g_main_pending())
          g_main_iteration(true);
        cleartry++;
      }

    super::TearDown();
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
  auto timezones = std::make_shared<Timezones>();
  auto clock = std::make_shared<LiveClock>(timezones);
  Snap snap (clock, settings);

  // GetCapabilities returns an array containing 'actions',
  // so our snap decision will be interactive.
  // For this test, it means we should get a timeout Notify Hint
  // that matches duration_minutes
  GError * error = nullptr;
  dbus_test_dbus_mock_object_add_method(mock, obj, METHOD_GET_CAPS, nullptr, G_VARIANT_TYPE_STRING_ARRAY, "ret = ['actions', 'body']", &error);
  g_assert_no_error (error);

  // call the Snap Decision
  auto func = [this](const Appointment&){g_idle_add(quit_idle, loop);};
  snap(appt, func, func);

  // confirm that Notify got called once
  guint len = 0;
  auto calls = dbus_test_dbus_mock_object_get_method_calls (mock, obj, METHOD_NOTIFY, &len, &error);
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
}

