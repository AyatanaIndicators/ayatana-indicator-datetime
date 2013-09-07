/*
Copyright 2012 Canonical Ltd.

Authors:
    Charles Kerr <charles.kerr@canonical.com>

This program is free software: you can redistribute it and/or modify it 
under the terms of the GNU General Public License version 3, as published 
by the Free Software Foundation.

This program is distributed in the hope that it will be useful, but 
WITHOUT ANY WARRANTY; without even the implied warranties of 
MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR 
PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along 
with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <gtest/gtest.h>

#include <glib-object.h>

#include "utils.h"

/***
****
***/

TEST (UtilsTest, SplitSettingsLocation)
{
  guint i;
  guint n;

  struct {
    const char * location;
    const char * expected_zone;
    const char * expected_name;
  } test_cases[] = {
    { "America/Chicago Chicago", "America/Chicago", "Chicago" },
    { "America/Chicago Oklahoma City", "America/Chicago", "Oklahoma City" },
    { "America/Los_Angeles",      "America/Los_Angeles", "Los Angeles" },
    { "America/Los_Angeles  ",    "America/Los_Angeles", "Los Angeles" },
    { "  America/Los_Angeles",    "America/Los_Angeles", "Los Angeles" },
    { "  America/Los_Angeles   ", "America/Los_Angeles", "Los Angeles" },
    { "UTC UTC", "UTC", "UTC" }
  };

  for (i=0, n=G_N_ELEMENTS(test_cases); i<n; i++)
    {
      char * zone = NULL;
      char * name = NULL;

      split_settings_location (test_cases[i].location, &zone, &name);
      ASSERT_STREQ (test_cases[i].expected_zone, zone);
      ASSERT_STREQ (test_cases[i].expected_name, name);

      g_free (zone);
      g_free (name);
    }
}

/***
****
***/

#define EM_SPACE "\xE2\x80\x82"

TEST (UtilsTest, GenerateTerseFormatString)
{
  guint i;
  guint n;
  GDateTime * arbitrary_day = g_date_time_new_local (2013, 6, 25, 12, 34, 56);
  GDateTime * on_the_hour   = g_date_time_new_local (2013, 6, 25, 12,  0,  0);

  struct {
    GDateTime * now;
    GDateTime * time;
    const char * expected_format_string;
  } test_cases[] = {
    { g_date_time_ref(arbitrary_day), g_date_time_ref(arbitrary_day), "%I:%M %p" }, /* identical time */
    { g_date_time_ref(arbitrary_day), g_date_time_add_hours(arbitrary_day,1), "%I:%M %p" }, /* later today */
    { g_date_time_ref(arbitrary_day), g_date_time_add_days(arbitrary_day,1), "Tomorrow" EM_SPACE "%I:%M %p" }, /* tomorrow */
    { g_date_time_ref(arbitrary_day), g_date_time_add_days(arbitrary_day,2), "%a" EM_SPACE "%I:%M %p" },
    { g_date_time_ref(arbitrary_day), g_date_time_add_days(arbitrary_day,6), "%a" EM_SPACE "%I:%M %p" },
    { g_date_time_ref(arbitrary_day), g_date_time_add_days(arbitrary_day,7), "%d %b" EM_SPACE "%I:%M %p" }, /* over one week away */

    { g_date_time_ref(on_the_hour), g_date_time_ref(on_the_hour), "%l %p" }, /* identical time */
    { g_date_time_ref(on_the_hour), g_date_time_add_hours(on_the_hour,1), "%l %p" }, /* later today */
    { g_date_time_ref(on_the_hour), g_date_time_add_days(on_the_hour,1), "Tomorrow" EM_SPACE "%l %p" }, /* tomorrow */
    { g_date_time_ref(on_the_hour), g_date_time_add_days(on_the_hour,2), "%a" EM_SPACE "%l %p" },
    { g_date_time_ref(on_the_hour), g_date_time_add_days(on_the_hour,6), "%a" EM_SPACE "%l %p" },
    { g_date_time_ref(on_the_hour), g_date_time_add_days(on_the_hour,7), "%d %b" EM_SPACE "%l %p" }, /* over one week away */
  };

  for (i=0, n=G_N_ELEMENTS(test_cases); i<n; i++)
    {
      char * format_string;

      format_string = generate_terse_format_string_at_time (test_cases[i].now,
                                                            test_cases[i].time); 

      ASSERT_STREQ (test_cases[i].expected_format_string, format_string);

      g_free (format_string);
      g_date_time_unref (test_cases[i].now);
      g_date_time_unref (test_cases[i].time);
    }

  g_date_time_unref (arbitrary_day);
  g_date_time_unref (on_the_hour);
}
