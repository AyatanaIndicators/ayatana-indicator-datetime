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

#include <datetime/appointment.h>
#include <datetime/settings.h>
#include <datetime/snap.h>

#include "notification-fixture.h"

/***
****
***/

using namespace ayatana::indicator::datetime;

namespace
{
  static constexpr char const * APP_NAME {"indicator-datetime-service"};

  gboolean quit_idle (gpointer gloop)
  {
    g_main_loop_quit(static_cast<GMainLoop*>(gloop));
    return G_SOURCE_REMOVE;
  }
}

/***
****
***/

TEST_F(NotificationFixture,Notification)
{
  // Feed different combinations of system settings,
  // indicator-datetime settings, and event types,
  // then see if notifications and haptic feedback behave as expected.

  auto settings = std::make_shared<Settings>();
  auto ne = std::make_shared<ayatana::indicator::notifications::Engine>(APP_NAME);
  auto sb = std::make_shared<ayatana::indicator::notifications::DefaultSoundBuilder>();
  auto func = [this](const Appointment&, const Alarm&){g_idle_add(quit_idle, loop);};

  // combinatorial factor #1: event type
  struct {
    Appointment appt;
    const char* icon_name;
    const char* prefix;
    bool expected_notify_called;
    bool expected_vibrate_called;
  } test_appts[] = {
    { appt, "calendar", "Event", true, true },
    { ualarm, "alarm-clock", "Alarm", true, true }
  };

  // combinatorial factor #2: indicator-datetime's haptic mode
  struct {
    const char* haptic_mode;
    bool expected_notify_called;
    bool expected_vibrate_called;
  } test_haptics[] = {
    { "none", true, false },
    { "pulse", true, true }
  };

  // combinatorial factor #3: system settings' "other vibrations" enabled
  struct {
    bool other_vibrations;
    bool expected_notify_called;
    bool expected_vibrate_called;
  } test_other_vibrations[] = {
    { true, true, true },
    { false, true, false }
  };

  // combinatorial factor #4: system settings' notifications app blacklist
  const std::set<std::pair<std::string,std::string>> blacklist_calendar { std::make_pair(std::string{"com.lomiri.calendar"}, std::string{"calendar-app"}) };
  const std::set<std::pair<std::string,std::string>> blacklist_empty;
  struct {
    std::set<std::pair<std::string,std::string>> muted_apps; // apps that should not trigger notifications
    std::set<Appointment::Type> expected_notify_called; // do we expect the notification to show?
    std::set<Appointment::Type> expected_vibrate_called; // do we expect the phone to vibrate?
  } test_muted_apps[] = {
    { blacklist_empty,    std::set<Appointment::Type>{ Appointment::Type::UBUNTU_ALARM, Appointment::Type::EVENT },
                          std::set<Appointment::Type>{ Appointment::Type::UBUNTU_ALARM, Appointment::Type::EVENT } },
    { blacklist_calendar, std::set<Appointment::Type>{ Appointment::Type::UBUNTU_ALARM },
                          std::set<Appointment::Type>{ Appointment::Type::UBUNTU_ALARM } }
  };

  for (const auto& test_appt : test_appts)
  {
  for (const auto& test_haptic : test_haptics)
  {
  for (const auto& test_vibes : test_other_vibrations)
  {
  for (const auto& test_muted : test_muted_apps)
  {
    const bool expected_notify_called = test_appt.expected_notify_called
                                     && test_vibes.expected_notify_called
                                     && (test_muted.expected_notify_called.count(test_appt.appt.type) > 0)
                                     && test_haptic.expected_notify_called;

    const bool expected_vibrate_called = test_appt.expected_vibrate_called
                                      && test_vibes.expected_vibrate_called
                                      && (test_muted.expected_vibrate_called.count(test_appt.appt.type) > 0)
                                      && test_haptic.expected_vibrate_called;

    // set test case properties: blacklist
    settings->muted_apps.set(test_muted.muted_apps);

    // set test case properties: haptic mode
    settings->alarm_haptic.set(test_haptic.haptic_mode);

    // set test case properties: other-vibrations flag
    // (and wait for the PropertiesChanged signal so we know the dbusmock got it)
    GError * error {};
    dbus_test_dbus_mock_object_update_property(as_mock,
                                               as_obj,
                                               PROP_OTHER_VIBRATIONS,
                                               g_variant_new_boolean(test_vibes.other_vibrations),
                                               &error);
    g_assert_no_error(error);

    // wait for previous iterations' bus noise to finish and reset the counters
    wait_msec(500);
    dbus_test_dbus_mock_object_clear_method_calls(haptic_mock, haptic_obj, &error);
    dbus_test_dbus_mock_object_clear_method_calls(notify_mock, notify_obj, &error);
    g_assert_no_error(error);

    // run the test
    auto snap = create_snap(ne, sb, settings);
    (*snap)(test_appt.appt, appt.alarms.front(), func, func);

    // confirm that the notification was as expected
    if (expected_notify_called) {
      EXPECT_METHOD_CALLED_EVENTUALLY(notify_mock, notify_obj, METHOD_NOTIFY);
    } else {
      EXPECT_METHOD_NOT_CALLED_EVENTUALLY(notify_mock, notify_obj, METHOD_NOTIFY);
    }

    // confirm that the vibration was as expected
    if (expected_vibrate_called) {
      EXPECT_METHOD_CALLED_EVENTUALLY(haptic_mock, haptic_obj, HAPTIC_METHOD_VIBRATE_PATTERN);
    } else {
      EXPECT_METHOD_NOT_CALLED_EVENTUALLY(haptic_mock, haptic_obj, HAPTIC_METHOD_VIBRATE_PATTERN);
    }

    // confirm that the notification was as expected
    guint num_notify_calls = 0;
    const auto notify_calls = dbus_test_dbus_mock_object_get_method_calls(notify_mock,
                                                                          notify_obj,
                                                                          METHOD_NOTIFY,
                                                                          &num_notify_calls,
                                                                          &error);
    g_assert_no_error(error);
    if (num_notify_calls > 0)
    {
        // test that Notify was called with the app_name
        const gchar* app_name {nullptr};
        g_variant_get_child(notify_calls[0].params, 0, "&s", &app_name);
        ASSERT_STREQ(APP_NAME, app_name);

        // test that Notify was called with the type-appropriate icon
        const gchar* icon_name {nullptr};
        g_variant_get_child(notify_calls[0].params, 2, "&s", &icon_name);
        ASSERT_STREQ(test_appt.icon_name, icon_name);

        // test that the Notification title has the correct prefix
        const gchar* title {nullptr};
        g_variant_get_child(notify_calls[0].params, 3, "&s", &title);
        ASSERT_TRUE(g_str_has_prefix(title, test_appt.prefix));

        // test that Notify was called with the appointment's body
        const gchar* body {nullptr};
        g_variant_get_child(notify_calls[0].params, 4, "&s", &body);
        ASSERT_STREQ(test_appt.appt.summary.c_str(), body);
    }
  }
  }
  }
  }
}

