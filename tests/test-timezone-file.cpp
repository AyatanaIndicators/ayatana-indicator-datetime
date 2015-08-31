
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

#include "timedated-fixture.h"

#include <datetime/timezone-file.h>

#include <cstdio> // fopen()
//#include <sys/stat.h> // chmod()
#include <unistd.h> // sync()

using unity::indicator::datetime::FileTimezone;

/**
 * Test that timezone-file picks up the initial value
 */
TEST_F(TimedateFixture, InitialValue)
{
  const std::string expected_timezone = "America/Chicago";
  set_timezone(expected_timezone);
  FileTimezone tz;
  ASSERT_EQ(expected_timezone, tz.timezone.get());
}

/**
 * Test that changing the tz after we are running works.
 */
TEST_F(TimedateFixture, ChangedValue)
{
  const std::string initial_timezone = "America/Chicago";
  const std::string changed_timezone = "America/New_York";
  GMainLoop *l = g_main_loop_new(nullptr, FALSE);

  set_timezone(initial_timezone);

  FileTimezone tz;
  ASSERT_EQ(initial_timezone, tz.timezone.get());

  bool changed = false;
  tz.timezone.changed().connect(
        [&changed, this, l](const std::string& s){
          g_message("timezone changed to %s", s.c_str());
          changed = true;
          g_main_loop_quit(l);
        });

  g_idle_add([](gpointer gself){
    static_cast<TimedateFixture*>(gself)->set_timezone("America/New_York");
    return G_SOURCE_REMOVE;
  }, this);

  g_main_loop_run(l);

  ASSERT_TRUE(changed);
  ASSERT_EQ(changed_timezone, tz.timezone.get());
}
