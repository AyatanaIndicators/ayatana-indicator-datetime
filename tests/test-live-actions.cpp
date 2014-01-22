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

#include <datetime/actions-live.h>

#include "state-mock.h"
#include "glib-fixture.h"

/***
****
***/

class MockLiveActions: public LiveActions
{
public:
    std::string last_cmd;
    std::string last_url;
    MockLiveActions(const std::shared_ptr<State>& state_in): LiveActions(state_in) {}
    virtual ~MockLiveActions() {}

protected:
    void dispatch_url(const std::string& url) { last_url = url; }
    void execute_command(const std::string& cmd) { last_cmd = cmd; }
};

/***
****
***/

using namespace unity::indicator::datetime;

class LiveActionsFixture: public GlibFixture
{
private:

    typedef GlibFixture super;

    static void on_bus_acquired(GDBusConnection* conn,
                                const gchar* name,
                                gpointer gself)
    {
        auto self = static_cast<LiveActionsFixture*>(gself);
        g_debug("bus acquired: %s, connection is %p", name, conn);

        // Set up a mock GSD.
        // All it really does is wait for calls to GetDevice and
        // returns the get_devices_retval variant
        static const GDBusInterfaceVTable vtable = {
            timedate1_handle_method_call,
            nullptr, /* GetProperty */
            nullptr, /* SetProperty */
        };

        self->connection = G_DBUS_CONNECTION(g_object_ref(G_OBJECT(conn)));

        GError* error = nullptr;
        self->object_register_id = g_dbus_connection_register_object(
            conn,
            "/org/freedesktop/timedate1",
            self->node_info->interfaces[0],
            &vtable,
            self,
            nullptr,
            &error);
        g_assert_no_error(error);
    }

    static void on_name_acquired(GDBusConnection* /*conn*/,
                                 const gchar*     /*name*/,
                                 gpointer           gself)
    {
        auto self = static_cast<LiveActionsFixture*>(gself);
        self->name_acquired = true;
        g_main_loop_quit(self->loop);
    }

    static void on_name_lost(GDBusConnection* /*conn*/,
                             const gchar*     /*name*/,
                             gpointer           gself)
    {
        auto self = static_cast<LiveActionsFixture*>(gself);
        self->name_acquired = false;
    }

    static void on_bus_closed(GObject*       /*object*/,
                              GAsyncResult*    res,
                              gpointer         gself)
    {
        auto self = static_cast<LiveActionsFixture*>(gself);
        GError* err = nullptr;
        g_dbus_connection_close_finish(self->connection, res, &err);
        g_assert_no_error(err);
        g_main_loop_quit(self->loop);
    }

    static void
    timedate1_handle_method_call(GDBusConnection       * /*connection*/,
                                 const gchar           * /*sender*/,
                                 const gchar           * /*object_path*/,
                                 const gchar           * /*interface_name*/,
                                 const gchar           *   method_name,
                                 GVariant              *   parameters,
                                 GDBusMethodInvocation *   invocation,
                                 gpointer                  gself)
    {
        g_assert(!g_strcmp0(method_name, "SetTimezone"));
        g_assert(g_variant_is_of_type(parameters, G_VARIANT_TYPE_TUPLE));
        g_assert(2 == g_variant_n_children(parameters));

        auto child = g_variant_get_child_value(parameters, 0);
        g_assert(g_variant_is_of_type(child, G_VARIANT_TYPE_STRING));
        auto self = static_cast<LiveActionsFixture*>(gself);
        self->attempted_tzid = g_variant_get_string(child, nullptr);
        g_variant_unref(child);

        g_dbus_method_invocation_return_value(invocation, nullptr);
        g_main_loop_quit(self->loop);
    }

protected:

    std::shared_ptr<MockState> m_mock_state;
    std::shared_ptr<State> m_state;
    std::shared_ptr<MockLiveActions> m_live_actions;
    std::shared_ptr<Actions> m_actions;

    bool name_acquired;
    std::string attempted_tzid;

    GTestDBus* bus;
    guint own_name;
    GDBusConnection* connection;
    GDBusNodeInfo* node_info;
    int object_register_id;

    void SetUp()
    {
        super::SetUp();

        name_acquired = false;
        attempted_tzid.clear();
        connection = nullptr;
        node_info = nullptr;
        object_register_id = 0;
        own_name = 0;

        // bring up the test bus
        bus = g_test_dbus_new(G_TEST_DBUS_NONE);
        g_test_dbus_up(bus);
        const auto address = g_test_dbus_get_bus_address(bus);
        g_setenv("DBUS_SYSTEM_BUS_ADDRESS", address, true);
        g_setenv("DBUS_SESSION_BUS_ADDRESS", address, true);
        g_debug("test_dbus's address is %s", address);

        // parse the org.freedesktop.timedate1 interface
        const gchar introspection_xml[] =
            "<node>"
            "  <interface name='org.freedesktop.timedate1'>"
            "    <method name='SetTimezone'>"
            "      <arg name='timezone' type='s' direction='in'/>"
            "      <arg name='user_interaction' type='b' direction='in'/>"
            "    </method>"
            "  </interface>"
            "</node>";
        node_info = g_dbus_node_info_new_for_xml(introspection_xml, nullptr);
        ASSERT_TRUE(node_info != nullptr);
        ASSERT_TRUE(node_info->interfaces != nullptr);
        ASSERT_TRUE(node_info->interfaces[0] != nullptr);
        ASSERT_TRUE(node_info->interfaces[1] == nullptr);
        ASSERT_STREQ("org.freedesktop.timedate1", node_info->interfaces[0]->name);

        // own the bus
        own_name = g_bus_own_name(G_BUS_TYPE_SYSTEM,
                                  "org.freedesktop.timedate1",
                                  G_BUS_NAME_OWNER_FLAGS_NONE,
                                  on_bus_acquired, on_name_acquired, on_name_lost,
                                  this, nullptr);
        ASSERT_TRUE(object_register_id == 0);
        ASSERT_FALSE(name_acquired);
        ASSERT_TRUE(connection == nullptr);
        g_main_loop_run(loop);
        ASSERT_TRUE(object_register_id != 0);
        ASSERT_TRUE(name_acquired);
        ASSERT_TRUE(G_IS_DBUS_CONNECTION(connection));

        // create the State and Actions
        m_mock_state.reset(new MockState);
        m_mock_state->settings.reset(new Settings);
        m_state = std::dynamic_pointer_cast<State>(m_mock_state);
        m_live_actions.reset(new MockLiveActions(m_state));
        m_actions = std::dynamic_pointer_cast<Actions>(m_live_actions);
    }

    void TearDown()
    {
        m_actions.reset();
        m_live_actions.reset();
        m_state.reset();
        m_mock_state.reset();

        g_dbus_connection_unregister_object(connection, object_register_id);
        g_dbus_node_info_unref(node_info);
        g_bus_unown_name(own_name);
        g_dbus_connection_close(connection, nullptr, on_bus_closed, this);
        g_main_loop_run(loop);
        g_clear_object(&connection);
        g_test_dbus_down(bus);
        g_clear_object(&bus);

        super::TearDown();
    }
};

/***
****
***/

TEST_F(LiveActionsFixture, HelloWorld)
{
    EXPECT_TRUE(true);
}

TEST_F(LiveActionsFixture, SetLocation)
{
    const std::string tzid = "America/Chicago";
    const std::string name = "Oklahoma City";
    const std::string expected = tzid + " " + name;

    EXPECT_NE(expected, m_state->settings->timezone_name.get());

    m_actions->set_location(tzid, name);
    g_main_loop_run(loop);
    EXPECT_EQ(attempted_tzid, tzid);
    wait_msec();

    EXPECT_EQ(expected, m_state->settings->timezone_name.get());
}

TEST_F(LiveActionsFixture, OpenDesktopSettings)
{
    m_actions->open_desktop_settings();
    const std::string expected_substr = "control-center";
    EXPECT_NE(m_live_actions->last_cmd.find(expected_substr), std::string::npos);
}

TEST_F(LiveActionsFixture, OpenPlanner)
{
    m_actions->open_planner();
    const std::string expected = "evolution -c calendar";
    EXPECT_EQ(expected, m_live_actions->last_cmd);
}

TEST_F(LiveActionsFixture, OpenPhoneSettings)
{
    m_actions->open_phone_settings();
    const std::string expected = "settings:///system/time-date";
    EXPECT_EQ(expected, m_live_actions->last_url);
}

TEST_F(LiveActionsFixture, OpenPhoneClockApp)
{
    m_actions->open_phone_clock_app();
    const std::string expected = "appid://com.ubuntu.clock/clock/current-user-version";
    EXPECT_EQ(expected, m_live_actions->last_url);
}

TEST_F(LiveActionsFixture, OpenPlannerAt)
{
    const auto now = DateTime::NowLocal();
    m_actions->open_planner_at(now);
    const std::string expected = now.format("evolution \"calendar:///?startdate=%Y%m%d\"");
    EXPECT_EQ(expected, m_live_actions->last_cmd);
}
