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

#include "glib-fixture.h"

#include "Clock.h"
#include "MockClock.h"

/***
****
***/

using unity::indicator::datetime::Clock;
using unity::indicator::datetime::MockClock;
using unity::indicator::datetime::SkewDetector;

class SkewFixture: public GlibFixture
{
  private:

    typedef GlibFixture super;

    static void
    on_bus_opened (GObject * o G_GNUC_UNUSED, GAsyncResult * res, gpointer gself)
    {
      auto self = static_cast<SkewFixture*>(gself);

      GError * err = 0;
      self->system_bus = g_bus_get_finish (res, &err);
      g_assert_no_error (err);

      g_main_loop_quit (self->loop);
    }

    static void
    on_bus_closed (GObject * o G_GNUC_UNUSED, GAsyncResult * res, gpointer gself)
    {
      auto self = static_cast<SkewFixture*>(gself);

      GError * err = 0;
      g_dbus_connection_close_finish (self->system_bus, res, &err);
      g_assert_no_error (err);

      g_main_loop_quit (self->loop);
    }

  protected:

    std::shared_ptr<Clock> mockClock;
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

      // create a clock
      GDateTime * now = g_date_time_new_now_local ();
      mockClock.reset (new MockClock (now));
      g_date_time_unref (now);
    }

    virtual void TearDown ()
    {
      mockClock.reset();

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


/**
 * A simple "hello world" style test.
 */
TEST_F (SkewFixture, CanInstantiate)
{
    SkewDetector skew (std::dynamic_pointer_cast<Clock>(mockClock));
    wait_msec (500); // wait for the bus to set up
}


/**
 * Confirm that changing the clock's timezone triggers a skew event
 */
TEST_F (SkewFixture, ChangingTimezonesTriggersEvent)
{
    SkewDetector skew (std::dynamic_pointer_cast<Clock>(mockClock));
    wait_msec (500); // wait for the bus to set up

    bool skewed = false;
    skew.skewDetected.connect([&skewed, this](){
                    skewed = true;
                    g_main_loop_quit(loop);
                    return G_SOURCE_REMOVE;
                });

    g_idle_add([](gpointer gclock){
                    GDateTime * arbitrary = g_date_time_new_local (2020, 10, 31, 18, 30, 59);
                    static_cast<MockClock*>(gclock)->setLocaltime (arbitrary);
                    g_date_time_unref (arbitrary);
                    return G_SOURCE_REMOVE;
                }, mockClock.get());

    wait_msec (1000);

    EXPECT_TRUE (skewed);
    GDateTime * expected = g_date_time_new_local (2020, 10, 31, 18, 30, 59);
    GDateTime * actual = mockClock->localtime();
    EXPECT_EQ (0, g_date_time_compare (expected, actual));
    g_date_time_unref (actual);
    g_date_time_unref (expected);
}

/**
 * Confirm that a "PrepareForSleep" event wil trigger a skew event
 */
TEST_F (SkewFixture, PrepareForSleep)
{
    SkewDetector skew (std::dynamic_pointer_cast<Clock>(mockClock));
    wait_msec (500); // wait for the bus to set up

    bool skewed = false;
    skew.skewDetected.connect([&skewed, this](){
                    skewed = true;
                    g_main_loop_quit(loop);
                    return G_SOURCE_REMOVE;
                });

    g_idle_add ([](gpointer gself){
                    static_cast<SkewFixture*>(gself)->emitPrepareForSleep();
                    return G_SOURCE_REMOVE;
                }, this);

    wait_msec (1000);
    EXPECT_TRUE(skewed);
}


/**
 * Confirm that normal time passing doesn't trigger a skew event.
 * that idling changing the clock's time triggers a skew event
 */
TEST_F (SkewFixture, IdleDoesNotTriggerEvent)
{
    SkewDetector skew (std::dynamic_pointer_cast<Clock>(mockClock));
    wait_msec (500); // wait for the bus to set up

    bool skewed = false;
    skew.skewDetected.connect([&skewed](){
                    skewed = true;
                    g_warn_if_reached();
                    //abort();
                    return G_SOURCE_REMOVE;
                });

    const unsigned int intervalSec = 4;
    skew.intervalSec.set(intervalSec);
    wait_msec (intervalSec * 2.5 * 1000);
    EXPECT_FALSE (skewed);
}
