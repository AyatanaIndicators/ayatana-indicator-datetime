/*
 * Copyright © 2014-2016 Canonical Ltd.
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
 *   Ted Gould <ted.gould@canonical.com>
 */

#include "glib-fixture.h"

#include <datetime/dbus-shared.h>
#include <datetime/timezone-timedated.h>

#include <gio/gio.h>


using namespace ayatana::indicator::datetime;


struct Timedate1Fixture: public GlibFixture
{
private:

    typedef GlibFixture super;

protected:

    GDBusConnection* m_bus {};
    GTestDBus* m_test_bus {};

    void SetUp() override
    {
        super::SetUp();

        // use a fake bus
        m_test_bus = g_test_dbus_new(G_TEST_DBUS_NONE);
        g_test_dbus_up(m_test_bus);
        const char * address = g_test_dbus_get_bus_address(m_test_bus);
        g_setenv("DBUS_SYSTEM_BUS_ADDRESS", address, true);
        g_setenv("DBUS_SESSION_BUS_ADDRESS", address, true);
        g_debug("test_dbus's address is %s", address);

        // get the bus
        m_bus = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, nullptr);
        g_dbus_connection_set_exit_on_close(m_bus, FALSE);
        g_object_add_weak_pointer(G_OBJECT(m_bus), (gpointer*)&m_bus);
    }

    void TearDown() override
    {
        g_clear_object(&m_bus);
        g_clear_object(&m_test_bus);

        super::TearDown();
    }

    void start_timedate1(const std::string& tzid)
    {
        // start the store
        auto json_parameters = g_strdup_printf("{\"Timezone\": \"%s\"}", tzid.c_str());
        const gchar* child_argv[] = { "python3", "-m", "dbusmock", "--template", "timedated", "--parameters", json_parameters, nullptr };
        GError* error = nullptr;
        g_spawn_async(nullptr, (gchar**)child_argv, nullptr, G_SPAWN_SEARCH_PATH, nullptr, nullptr, nullptr, &error);
        g_assert_no_error(error);
        g_free(json_parameters);

        // wait for it to appear on the bus
        wait_for_name_owned(m_bus, Bus::Timedate1::BUSNAME);
    }

    bool wait_for_tzid(const std::string& tzid, Timezone& tz)
    {
        return wait_for([&tzid, &tz](){return tzid == tz.timezone.get();});
    }

    void set_timedate1_timezone(const std::string& tzid)
    {
        GError* error {};
        auto v = g_dbus_connection_call_sync(
            m_bus,
            Bus::Timedate1::BUSNAME,
            Bus::Timedate1::ADDR,
            Bus::Timedate1::IFACE,
            Bus::Timedate1::Methods::SET_TIMEZONE,
            g_variant_new("(sb)", tzid.c_str(), FALSE),
            nullptr,
            G_DBUS_CALL_FLAGS_NONE,
            -1,
            nullptr,
            &error);
        g_clear_pointer(&v, g_variant_unref);
        g_assert_no_error(error);
    }
};

#define EXPECT_TZID(expected_tzid, tmp) \
    EXPECT_TRUE(wait_for_tzid(expected_tzid, tmp)) \
        << "expected " << expected_tzid \
        << " got " << tmp.timezone.get();

/***
****
***/

TEST_F(Timedate1Fixture, HelloWorld)
{
}

/**
 * Test that the tzid is right if timedated isn't available
 */
TEST_F(Timedate1Fixture, DefaultTimezone)
{
    const std::string expected_tzid{"Etc/Utc"};

    TimedatedTimezone tmp;
    EXPECT_TZID(expected_tzid, tmp);
}

/**
 * Test that the tzid is right if timedated shows BEFORE we start
 */
TEST_F(Timedate1Fixture, Timedate1First)
{
    const std::string expected_tzid{"America/Chicago"};

    start_timedate1(expected_tzid);
    TimedatedTimezone tmp;
    EXPECT_TZID(expected_tzid, tmp);
}

/**
 * Test that the tzid is right if timedated shows AFTER we start
 */
TEST_F(Timedate1Fixture, Timedate1Last)
{
    const std::string expected_tzid("America/Los_Angeles");

    TimedatedTimezone tmp;
    start_timedate1(expected_tzid);
    EXPECT_TZID(expected_tzid, tmp);
}

/**
 * Test that the tzid is right if timedated's property changes
 */
TEST_F(Timedate1Fixture, TimezoneChange)
{
    const std::vector<std::string> expected_tzids{"America/Los_Angeles", "America/Chicago", "Etc/Utc"};

    TimedatedTimezone tmp;
    start_timedate1("America/New_York");

    for(const auto& expected_tzid : expected_tzids)
    {
        set_timedate1_timezone(expected_tzid);
        EXPECT_TZID(expected_tzid, tmp);
    }
}
