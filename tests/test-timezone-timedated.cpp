
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

#include <datetime/timezone-timedated.h>

using unity::indicator::datetime::TimedatedTimezone;

/***
****
***/

#define TIMEZONE_FILE (SANDBOX"/timezone")

class TimezoneFixture: public TimedateFixture
{
  private:

    typedef TimedateFixture super;

  protected:

    void SetUp() override
    {
      super::SetUp();
    }

    void TearDown() override
    {
      super::TearDown();
    }

  public:

    /* convenience func to set the timezone file */
    void set_file(const std::string& text)
    {
      g_debug("set_file %s %s", TIMEZONE_FILE, text.c_str());
      auto fp = fopen(TIMEZONE_FILE, "w+");
      fprintf(fp, "%s\n", text.c_str());
      fclose(fp);
      sync();
    }
};

/**
 * Test that timezone-timedated warns, but doesn't crash, if the timezone file doesn't exist
 */
TEST_F(TimezoneFixture, NoFile)
{
  remove(TIMEZONE_FILE);
  ASSERT_FALSE(g_file_test(TIMEZONE_FILE, G_FILE_TEST_EXISTS));

  expectLogMessage(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "*No such file or directory*");
  TimedatedTimezone tz(TIMEZONE_FILE);
}

/**
 * Test that timezone-timedated gives a default of UTC if the file doesn't exist
 */
TEST_F(TimezoneFixture, DefaultValueNoFile)
{
  const std::string expected_timezone = "Etc/Utc";
  remove(TIMEZONE_FILE);
  ASSERT_FALSE(g_file_test(TIMEZONE_FILE, G_FILE_TEST_EXISTS));

  expectLogMessage(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "*No such file or directory*");
  TimedatedTimezone tz(TIMEZONE_FILE);
  ASSERT_EQ(expected_timezone, tz.timezone.get());
}

/**
 * Test that timezone-timedated picks up the initial value
 */
TEST_F(TimezoneFixture, InitialValue)
{
  const std::string expected_timezone = "America/Chicago";
  set_file(expected_timezone);
  TimedatedTimezone tz(TIMEZONE_FILE);
}

/**
 * Test that changing the tz after we are running works.
 */
TEST_F(TimezoneFixture, ChangedValue)
{
  const std::string initial_timezone = "America/Chicago";
  const std::string changed_timezone = "America/New_York";

  set_file(initial_timezone);

  TimedatedTimezone tz(TIMEZONE_FILE);
  ASSERT_EQ(initial_timezone, tz.timezone.get());

  bool changed = false;
  tz.timezone.changed().connect(
        [&changed, this](const std::string& s){
          g_message("timezone changed to %s", s.c_str());
          changed = true;
          g_main_loop_quit(loop);
        });

  g_idle_add([](gpointer gself){
    static_cast<TimedateFixture*>(gself)->set_timezone("America/New_York");
    return G_SOURCE_REMOVE;
  }, this);

  g_main_loop_run(loop);

  ASSERT_TRUE(changed);
  ASSERT_EQ(changed_timezone, tz.timezone.get());
}

/**
 * Test that timezone-timedated picks up the initial value
 */
TEST_F(TimezoneFixture, IgnoreComments)
{
  const std::string comment = "# Created by cloud-init v. 0.7.5 on Thu, 24 Apr 2014 14:03:29 +0000";
  const std::string expected_timezone = "Europe/Berlin";
  set_file(comment + "\n" + expected_timezone);
  TimedatedTimezone tz(TIMEZONE_FILE);
  ASSERT_EQ(expected_timezone, tz.timezone.get());
}
