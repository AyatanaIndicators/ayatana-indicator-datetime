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

#include <datetime/clock.h>
#include <datetime/timezones.h>

#include "glib-fixture.h"

/***
****
***/

using unity::indicator::datetime::Clock;
using unity::indicator::datetime::LiveClock;
using unity::indicator::datetime::Timezones;

class ClockFixture: public GlibFixture
{
  private:

    typedef GlibFixture super;

    static void
    on_bus_opened (GObject * o G_GNUC_UNUSED, GAsyncResult * res, gpointer gself)
    {
      auto self = static_cast<ClockFixture*>(gself);

      GError * err = 0;
      self->system_bus = g_bus_get_finish (res, &err);
      g_assert_no_error (err);

      g_main_loop_quit (self->loop);
    }

    static void
    on_bus_closed (GObject * o G_GNUC_UNUSED, GAsyncResult * res, gpointer gself)
    {
      auto self = static_cast<ClockFixture*>(gself);

      GError * err = 0;
      g_dbus_connection_close_finish (self->system_bus, res, &err);
      g_assert_no_error (err);

      g_main_loop_quit (self->loop);
    }

  protected:

    GTestDBus * test_dbus;
    GDBusConnection * system_bus;

    virtual void SetUp ()
    {
      super::SetUp ();

      // pull up a test dbus
      test_dbus = g_test_dbus_new (G_TEST_DBUS_NONE);
      g_test_dbus_up (test_dbus);
      const char * address = g_test_dbus_get_bus_address (test_dbus);
      g_setenv ("DBUS_SYSTEM_BUS_ADDRESS", address, TRUE);
      g_debug ("test_dbus's address is %s", address);

      // wait for the GDBusConnection before returning
      g_bus_get (G_BUS_TYPE_SYSTEM, nullptr, on_bus_opened, this);
      g_main_loop_run (loop);
    }

    virtual void TearDown ()
    {
      // close the system bus
      g_dbus_connection_close (system_bus, nullptr, on_bus_closed, this);
      g_main_loop_run (loop);
      g_clear_object (&system_bus);

      // tear down the test dbus
      g_test_dbus_down (test_dbus);
      g_clear_object (&test_dbus);

      super::TearDown ();
    }

  public:

    void emitPrepareForSleep ()
    {
      g_dbus_connection_emit_signal (g_bus_get_sync (G_BUS_TYPE_SYSTEM, nullptr, nullptr),
                                     NULL,
                                     "/org/freedesktop/login1", // object path
                                     "org.freedesktop.login1.Manager", // interface
                                     "PrepareForSleep", // signal name
                                     g_variant_new("(b)", FALSE),
                                     NULL);
    }
};

/***
****
***/

#define TIMEZONE_FILE (SANDBOX"/timezone")

TEST_F (ClockFixture, HelloFixture)
{
    std::shared_ptr<Timezones> zones (new Timezones);
    zones->timezone.set("America/New_York");
    LiveClock clock (zones);

#if 0
    GTimeZone * tz_nyc = g_time_zone_new (file_timezone.c_str());
    GDateTime * now_nyc = g_date_time_new_now (tz_nyc);
    GDateTime * now = clock.localtime();
    EXPECT_EQ (g_date_time_get_utc_offset(now_nyc), g_date_time_get_utc_offset(now));
    EXPECT_LE (abs(g_date_time_difference(now_nyc,now)), G_USEC_PER_SEC);
    g_date_time_unref (now);
    g_date_time_unref (now_nyc);
    g_time_zone_unref (tz_nyc);

    /// change the timezones!
    clock.skewDetected.connect([this](){
                    g_main_loop_quit(loop);
                });
    file_timezone = "America/Los_Angeles";
    g_idle_add ([](gpointer str){
                    set_file(static_cast<const char*>(str));
                    return G_SOURCE_REMOVE;
                }, const_cast<char*>(file_timezone.c_str()));
    g_main_loop_run (loop);

    GTimeZone * tz_la = g_time_zone_new (file_timezone.c_str());
    GDateTime * now_la = g_date_time_new_now (tz_la);
    now = clock.localtime();
    EXPECT_EQ (g_date_time_get_utc_offset(now_la), g_date_time_get_utc_offset(now));
    EXPECT_LE (abs(g_date_time_difference(now_la,now)), G_USEC_PER_SEC);
    g_date_time_unref (now);
    g_date_time_unref (now_la);
    g_time_zone_unref (tz_la);
#endif
}


TEST_F (ClockFixture, TimezoneChangeTriggersSkew)
{
    std::shared_ptr<Timezones> zones (new Timezones);
    zones->timezone.set("America/New_York");
    LiveClock clock (zones);
    //std::string file_timezone = "America/New_York";
    //set_file (file_timezone);
    //std::shared_ptr<TimezoneDetector> detector (new TimezoneDetector(TIMEZONE_FILE));
    //LiveClock clock (detector);

    GTimeZone * tz_nyc = g_time_zone_new ("America/New_York");
    GDateTime * now_nyc = g_date_time_new_now (tz_nyc);
    GDateTime * now = clock.localtime();
    EXPECT_EQ (g_date_time_get_utc_offset(now_nyc), g_date_time_get_utc_offset(now));
    EXPECT_LE (abs(g_date_time_difference(now_nyc,now)), G_USEC_PER_SEC);
    g_date_time_unref (now);
    g_date_time_unref (now_nyc);
    g_time_zone_unref (tz_nyc);

    /// change the timezones!
    clock.skewDetected.connect([this](){
                    g_main_loop_quit(loop);
                });
    g_idle_add ([](gpointer gs){
                    static_cast<Timezones*>(gs)->timezone.set("America/Los_Angeles");
                    return G_SOURCE_REMOVE;
                }, zones.get());
    g_main_loop_run (loop);

    GTimeZone * tz_la = g_time_zone_new ("America/Los_Angeles");
    GDateTime * now_la = g_date_time_new_now (tz_la);
    now = clock.localtime();
    EXPECT_EQ (g_date_time_get_utc_offset(now_la), g_date_time_get_utc_offset(now));
    EXPECT_LE (abs(g_date_time_difference(now_la,now)), G_USEC_PER_SEC);
    g_date_time_unref (now);
    g_date_time_unref (now_la);
    g_time_zone_unref (tz_la);
}

/**
 * Confirm that a "PrepareForSleep" event wil trigger a skew event
 */
TEST_F (ClockFixture, SleepTriggersSkew)
{
    std::shared_ptr<Timezones> zones (new Timezones);
    zones->timezone.set("America/New_York");
    LiveClock clock (zones);
    wait_msec (500); // wait for the bus to set up

    bool skewed = false;
    clock.skewDetected.connect([&skewed, this](){
                    skewed = true;
                    g_main_loop_quit(loop);
                    return G_SOURCE_REMOVE;
                });

    g_idle_add ([](gpointer gself){
                    static_cast<ClockFixture*>(gself)->emitPrepareForSleep();
                    return G_SOURCE_REMOVE;
                }, this);

    wait_msec (1000);
    EXPECT_TRUE(skewed);
}

/**
 * Confirm that normal time passing doesn't trigger a skew event.
 * that idling changing the clock's time triggers a skew event
 */
TEST_F (ClockFixture, IdleDoesNotTriggerSkew)
{
    std::shared_ptr<Timezones> zones (new Timezones);
    zones->timezone.set("America/New_York");
    LiveClock clock (zones);
    wait_msec (500); // wait for the bus to set up

    bool skewed = false;
    clock.skewDetected.connect([&skewed](){
                    skewed = true;
                    g_warn_if_reached();
                    return G_SOURCE_REMOVE;
                });

    const unsigned int intervalSec = 4;
    clock.skewTestIntervalSec.set(intervalSec);
    wait_msec (intervalSec * 2.5 * 1000);
    EXPECT_FALSE (skewed);
}
