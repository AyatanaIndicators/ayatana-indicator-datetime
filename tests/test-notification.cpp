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
  };
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
    bool expected_notify_called;
    bool expected_vibrate_called;
  } test_appts[] = {
    { appt, true, true },
    { ualarm, true, true }
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
    wait_msec(100);

    // run the test
    (*snap)(appt, appt.alarms.front(), func, func);
    wait_msec(100);

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

