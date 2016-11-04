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

#include "glib-fixture.h"

#include <libdbustest/dbus-test.h>

/***
****
***/

class LibdbusmockFixture: public GlibFixture
{
private:

  typedef GlibFixture super;

protected:

  GDBusConnection * system_bus {};
  GDBusConnection * session_bus {};
  DbusTestService * service {};

  void SetUp() override
  {

    super::SetUp();

    service = dbus_test_service_new(nullptr);
  }

  void startDbusMock()
  {
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
    g_clear_object(&service);
    g_object_unref(session_bus);
    g_object_unref(system_bus);

    // wait a little while for the scaffolding to shut down,
    // but don't block on it forever...
    wait_for([this](){return system_bus==nullptr && session_bus==nullptr;}, 5000);

    super::TearDown();
  }

  bool wait_for_method_call(DbusTestDbusMock* mock,
                            DbusTestDbusMockObject* obj,
                            const gchar* method,
                            GVariant* params=nullptr,
                            guint timeout_msec=100)
  {
    if (params != nullptr)
      g_variant_ref_sink(params);

    auto test_function = [mock, obj, method, params]() {
      GError* error {};
      const auto called = dbus_test_dbus_mock_object_check_method_call(mock,
                                                                       obj,
                                                                       method,
                                                                       params,
                                                                       &error);
      if (error != nullptr) {
        g_critical("Error looking for method call '%s': %s", method, error->message);
        g_clear_error(&error);
      }

      return called;
    };

    const auto ret = wait_for(test_function, timeout_msec);
    g_clear_pointer(&params, g_variant_unref);
    return ret;
  }

  void EXPECT_METHOD_CALLED_EVENTUALLY(DbusTestDbusMock* mock,
                                       DbusTestDbusMockObject* obj,
                                       const gchar* method,
                                       GVariant* params=nullptr,
                                       guint timeout_msec=1000)
  {
    EXPECT_TRUE(wait_for_method_call(mock, obj, method, params, timeout_msec)) << "method: " << method;
  }

  void EXPECT_METHOD_NOT_CALLED_EVENTUALLY(DbusTestDbusMock* mock,
                                           DbusTestDbusMockObject* obj,
                                           const gchar* method,
                                           GVariant* params=nullptr,
                                           guint timeout_msec=1000)
  {
    EXPECT_FALSE(wait_for_method_call(mock, obj, method, params, timeout_msec)) << "method: " << method;
  }

  void ASSERT_METHOD_CALLED_EVENTUALLY(DbusTestDbusMock* mock,
                                       DbusTestDbusMockObject* obj,
                                       const gchar* method,
                                       GVariant* params=nullptr,
                                       guint timeout_msec=1000)
  {
    ASSERT_TRUE(wait_for_method_call(mock, obj, method, params, timeout_msec)) << "method: " << method;
  }

  void ASSERT_METHOD_NOT_CALLED_EVENTUALLY(DbusTestDbusMock* mock,
                                           DbusTestDbusMockObject* obj,
                                           const gchar* method,
                                           GVariant* params=nullptr,
                                           guint timeout_msec=1000)
  {
    ASSERT_FALSE(wait_for_method_call(mock, obj, method, params, timeout_msec)) << "method: " << method;
  }
};

