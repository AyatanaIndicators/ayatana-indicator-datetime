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

#include "geoclue-fixture.h"

#include <datetime/timezone-geoclue.h>

//#include <libdbustest/dbus-test.h>

using unity::indicator::datetime::GeoclueTimezone;

/***
****
***/

class TimezoneGeoclueFixture : public GeoclueFixture
{
};

#if 0
namespace
{
  struct EmitAddressChangedData
  {
    DbusTestDbusMock * mock = nullptr;
    DbusTestDbusMockObject * obj_client = nullptr;
    std::string timezone;
    EmitAddressChangedData(DbusTestDbusMock * mock_,
                           DbusTestDbusMockObject * obj_client_,
                           const std::string& timezone_): mock(mock_), obj_client(obj_client_), timezone(timezone_) {}
  };

  gboolean emit_address_changed_idle(gpointer gdata)
  {
    auto data = static_cast<EmitAddressChangedData*>(gdata);

    GError * error = nullptr;
    dbus_test_dbus_mock_object_emit_signal(data->mock, data->obj_client,
                                            "org.freedesktop.Geoclue.Address",
                                            "AddressChanged",
                                            G_VARIANT_TYPE("(ia{ss}(idd))"),
                                            g_variant_new_parsed("(1385238033, {'timezone': 'America/Chicago'}, (3, 0.0, 0.0))"),
                                            &error);
    if (error)
      {
        g_warning("%s: %s", G_STRFUNC, error->message);
        g_error_free(error);
      }

    delete data;
    return G_SOURCE_REMOVE;
  }
}
#endif

TEST_F(TimezoneGeoclueFixture, ChangeDetected)
{
//  const std::string timezone_1 = "America/Denver";
  const std::string timezone_2 = "America/Chicago";

  GeoclueTimezone tz;
  wait_msec(500); // wait for the bus to get set up
  EXPECT_EQ(timezone_1, tz.timezone.get());

  // start listening for a timezone change, then change the timezone

  bool changed = false;
  auto connection = tz.timezone.changed().connect(
        [&changed, this](const std::string& s){
          g_debug("timezone changed to %s", s.c_str());
          changed = true;
          g_main_loop_quit(loop);
        });

  setGeoclueTimezoneOnIdle(timezone_2);
  //g_timeout_add(50, emit_address_changed_idle, new EmitAddressChangedData(mock, obj_client, timezone_2.c_str()));
  g_main_loop_run(loop);
  EXPECT_EQ(timezone_2, tz.timezone.get());
}


