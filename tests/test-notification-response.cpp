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

TEST_F(NotificationFixture,Response)
{
  // create the world
  make_interactive();
  auto ne = std::make_shared<ayatana::indicator::notifications::Engine>(APP_NAME);
  auto sb = std::make_shared<ayatana::indicator::notifications::DefaultSoundBuilder>();
  auto settings = std::make_shared<Settings>();
  int next_notification_id {FIRST_NOTIFY_ID};

  // set a response callback that remembers what response we got
  bool on_response_called {};
  Snap::Response response {};
  auto on_response = [this, &on_response_called, &response]
                     (const Appointment&, const Alarm&, const Snap::Response& r){
    on_response_called = true;
    response = r;
    g_idle_add(quit_idle, loop);
  };

  // our tests!
  const struct {
    Appointment appt;
    std::vector<std::string> expected_actions;
    std::string action;
    Snap::Response expected_response;
  } tests[] = {
    { appt,   {"show-app"},       "show-app", Snap::Response::ShowApp },
    { ualarm, {"none", "snooze"}, "snooze",   Snap::Response::Snooze  },
    { ualarm, {"none", "snooze"}, "none",     Snap::Response::None    }
  };


  settings->cal_notification_enabled.set(true);
  settings->cal_notification_sounds.set(true);
  settings->cal_notification_vibrations.set(true);
  settings->cal_notification_bubbles.set(true);
  settings->cal_notification_list.set(true);

  // walk through the tests
  for (const auto& test : tests)
  {
    // wait for previous iterations' bus noise to finish and reset the counters
    GError* error {};
    wait_msec(500);
    dbus_test_dbus_mock_object_clear_method_calls(notify_mock, notify_obj, &error);
    g_assert_no_error(error);
    on_response_called = false;

    // create a snap decision
    auto snap = create_snap(ne, sb, settings);
    (*snap)(test.appt, test.appt.alarms.front(), on_response);

    // wait for the notification to show up
    EXPECT_METHOD_CALLED_EVENTUALLY(notify_mock, notify_obj, METHOD_NOTIFY);

    // test that Notify was called the right number of times
    static constexpr int expected_num_notify_calls {1};
    guint num_notify_calls {};
    const auto notify_calls = dbus_test_dbus_mock_object_get_method_calls(
      notify_mock,
      notify_obj,
      METHOD_NOTIFY,
      &num_notify_calls,
      &error);
    g_assert_no_error(error);
    EXPECT_EQ(expected_num_notify_calls, num_notify_calls);

    // test that Notify was called with the correct list of actions
    if (num_notify_calls > 0) {
      std::vector<std::string> actions;
      const gchar** as {nullptr};
      g_variant_get_child(notify_calls[0].params, 5, "^a&s", &as);
      for (int i=0; as && as[i]; i+=2) // actions are in pairs of (name, i18n), skip the i18n
        actions.push_back(as[i]);
      EXPECT_EQ(test.expected_actions, actions);
    }

    // make the notification mock tell the world that the user invoked an action
    const auto notification_id = next_notification_id++;
    idle_add([this, notification_id, test](){
      GError* err {};
      dbus_test_dbus_mock_object_emit_signal(notify_mock, notify_obj, "ActionInvoked",
        G_VARIANT_TYPE("(us)"),
        g_variant_new("(us)", guint(notification_id), test.action.c_str()),
        &err);
      dbus_test_dbus_mock_object_emit_signal(notify_mock, notify_obj, "NotificationClosed",
        G_VARIANT_TYPE("(uu)"),
        g_variant_new("(uu)", guint(notification_id), guint(NOTIFICATION_CLOSED_DISMISSED)),
        &err);
      g_assert_no_error(err);
      return G_SOURCE_REMOVE;
    });

    // confirm that the response callback got the right response
    EXPECT_TRUE(wait_for([&on_response_called](){return on_response_called;}));
    EXPECT_EQ(int(test.expected_response), int(response)) << "notification_id " << notification_id;
  }
}
