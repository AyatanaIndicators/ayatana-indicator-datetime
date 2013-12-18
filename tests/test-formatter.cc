
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

#include "clock-mock.h"
#include "glib-fixture.h"

#include <datetime/formatter.h>
#include <datetime/settings-shared.h>

#include <glib/gi18n.h>

#include <langinfo.h>
#include <locale.h>

using unity::indicator::datetime::Clock;
using unity::indicator::datetime::MockClock;
using unity::indicator::datetime::PhoneFormatter;
using unity::indicator::datetime::DesktopFormatter;

/***
****
***/

class FormatterFixture: public GlibFixture
{
  private:

    typedef GlibFixture super;
    gchar * original_locale = nullptr;

  protected:

    GSettings * settings = nullptr;

    virtual void SetUp ()
    {
      super::SetUp ();

      settings = g_settings_new (SETTINGS_INTERFACE);
  
      original_locale = g_strdup (setlocale (LC_TIME, NULL));
    }

    virtual void TearDown ()
    {
      g_clear_object (&settings);

      setlocale (LC_TIME, original_locale);
      g_clear_pointer (&original_locale, g_free);

      super::TearDown ();
    }

    bool SetLocale (const char * expected_locale, const char * name)
    {
      setlocale (LC_TIME, expected_locale);
      const char * actual_locale = setlocale (LC_TIME, NULL);
      if (!g_strcmp0 (expected_locale, actual_locale))
        {
          return true;
        }
      else
        {
          g_warning ("Unable to set locale to %s; skipping %s locale tests.", expected_locale, name);
          return false;
        }
    }

    inline bool Set24hLocale () { return SetLocale ("C",          "24h"); }
    inline bool Set12hLocale () { return SetLocale ("en_US.utf8", "12h"); }
};


/**
 * Test the phone header format
 */
TEST_F (FormatterFixture, TestPhoneHeader)
{
    GDateTime * now = g_date_time_new_local (2020, 10, 31, 18, 30, 59);
    std::shared_ptr<MockClock> mock (new MockClock(now));
    std::shared_ptr<Clock> clock = std::dynamic_pointer_cast<Clock>(mock);

    // test the default value in a 24h locale
    if (Set24hLocale ())
    {
        PhoneFormatter formatter (clock);
        EXPECT_EQ (std::string("%H:%M"), formatter.headerFormat.get());
        EXPECT_EQ (std::string("18:30"), formatter.header.get());
    }

    // test the default value in a 12h locale
    if (Set12hLocale ())
    {
        PhoneFormatter formatter (clock);
        EXPECT_EQ (std::string("%l:%M %p"), formatter.headerFormat.get());
        EXPECT_EQ (std::string(" 6:30 PM"), formatter.header.get());
    }
}

#define EM_SPACE "\u2003"

/**
 * Test the default values of the desktop header format
 */
TEST_F (FormatterFixture, TestDesktopHeader)
{
  struct {
    bool is_12h;
    bool show_day;
    bool show_date;
    bool show_year;
    const char * expected_format_string;
  } test_cases[] = {
    { false, false, false, false, "%H:%M" },
    { false, false, false, true,  "%H:%M" }, // show_year is ignored iff show_date is false
    { false, false, true,  false, "%b %e" EM_SPACE "%H:%M" },
    { false, false, true,  true,  "%b %e %Y" EM_SPACE "%H:%M" },
    { false, true,  false, false, "%a" EM_SPACE "%H:%M" },
    { false, true,  false, true,  "%a" EM_SPACE "%H:%M" }, // show_year is ignored iff show_date is false
    { false, true,  true,  false, "%a %b %e" EM_SPACE "%H:%M" },
    { false, true,  true,  true,  "%a %b %e %Y" EM_SPACE "%H:%M" },
    { true,  false, false, false, "%l:%M %p" },
    { true,  false, false, true,  "%l:%M %p" }, // show_year is ignored iff show_date is false
    { true,  false, true,  false, "%b %e" EM_SPACE "%l:%M %p" },
    { true,  false, true,  true,  "%b %e %Y" EM_SPACE "%l:%M %p" },
    { true,  true,  false, false, "%a" EM_SPACE "%l:%M %p" },
    { true,  true,  false, true,  "%a" EM_SPACE "%l:%M %p" }, // show_year is ignored iff show_date is false
    { true,  true,  true,  false, "%a %b %e" EM_SPACE "%l:%M %p" },
    { true,  true,  true,  true,  "%a %b %e %Y" EM_SPACE "%l:%M %p" }
  };

  GDateTime * now = g_date_time_new_local (2020, 10, 31, 18, 30, 59);
  std::shared_ptr<MockClock> mock (new MockClock(now));
  std::shared_ptr<Clock> clock = std::dynamic_pointer_cast<Clock>(mock);

  for (int i=0, n=G_N_ELEMENTS(test_cases); i<n; i++)
    {
      if (test_cases[i].is_12h ? Set12hLocale() : Set24hLocale())
        {
          DesktopFormatter f (clock);

          g_settings_set_boolean (settings, SETTINGS_SHOW_DAY_S, test_cases[i].show_day);
          g_settings_set_boolean (settings, SETTINGS_SHOW_DATE_S, test_cases[i].show_date);
          g_settings_set_boolean (settings, SETTINGS_SHOW_YEAR_S, test_cases[i].show_year);

          ASSERT_STREQ (test_cases[i].expected_format_string, f.headerFormat.get().c_str());

          g_settings_reset (settings, SETTINGS_SHOW_DAY_S);
          g_settings_reset (settings, SETTINGS_SHOW_DATE_S);
          g_settings_reset (settings, SETTINGS_SHOW_YEAR_S);
        }
    }
}

/**
 * Test the default values of the desktop header format
 */
TEST_F (FormatterFixture, TestUpcomingTimes)
{
    GDateTime * a = g_date_time_new_local (2020, 10, 31, 18, 30, 59);

    struct {
        gboolean is_12h;
        GDateTime * now;
        GDateTime * then;
        GDateTime * then_end;
        const char * expected_format_string;
    } test_cases[] = {
        { true, g_date_time_ref(a), g_date_time_ref(a), NULL, "%l:%M %p" }, // identical time
        { true, g_date_time_ref(a), g_date_time_add_hours(a,1), NULL, "%l:%M %p" }, // later today
        { true, g_date_time_ref(a), g_date_time_add_days(a,1), NULL, "Tomorrow" EM_SPACE "%l:%M %p" }, // tomorrow
        { true, g_date_time_ref(a), g_date_time_add_days(a,2), NULL, "%a" EM_SPACE "%l:%M %p" },
        { true, g_date_time_ref(a), g_date_time_add_days(a,6), NULL, "%a" EM_SPACE "%l:%M %p" },
        { true, g_date_time_ref(a), g_date_time_add_days(a,7), NULL, "%a %d %b" EM_SPACE "%l:%M %p" }, // over one week away

        { false, g_date_time_ref(a), g_date_time_ref(a), NULL, "%H:%M" }, // identical time
        { false, g_date_time_ref(a), g_date_time_add_hours(a,1), NULL, "%H:%M" }, // later today
        { false, g_date_time_ref(a), g_date_time_add_days(a,1), NULL, "Tomorrow" EM_SPACE "%H:%M" }, // tomorrow
        { false, g_date_time_ref(a), g_date_time_add_days(a,2), NULL, "%a" EM_SPACE "%H:%M" },
        { false, g_date_time_ref(a), g_date_time_add_days(a,6), NULL, "%a" EM_SPACE "%H:%M" },
        { false, g_date_time_ref(a), g_date_time_add_days(a,7), NULL, "%a %d %b" EM_SPACE "%H:%M" } // over one week away
    };

    for (int i=0, n=G_N_ELEMENTS(test_cases); i<n; i++)
    {
        if (test_cases[i].is_12h ? Set12hLocale() : Set24hLocale())
        {
            std::shared_ptr<MockClock> mock (new MockClock(test_cases[i].now));
            std::shared_ptr<Clock> clock = std::dynamic_pointer_cast<Clock>(mock);
            DesktopFormatter f (clock);
        
            std::string fmt = f.getRelativeFormat (test_cases[i].then, test_cases[i].then_end);
            ASSERT_STREQ (test_cases[i].expected_format_string, fmt.c_str());

            g_clear_pointer (&test_cases[i].now, g_date_time_unref);
            g_clear_pointer (&test_cases[i].then, g_date_time_unref);
            g_clear_pointer (&test_cases[i].then_end, g_date_time_unref);
        }
    }

    g_date_time_unref (a);
}


/**
 * Test the default values of the desktop header format
 */
TEST_F (FormatterFixture, TestEventTimes)
{
    GDateTime * day            = g_date_time_new_local (2013, 1, 1, 13, 0, 0);
    GDateTime * day_begin      = g_date_time_new_local (2013, 1, 1, 13, 0, 0);
    GDateTime * day_end        = g_date_time_add_days (day_begin, 1);
    GDateTime * tomorrow_begin = g_date_time_add_days (day_begin, 1);
    GDateTime * tomorrow_end   = g_date_time_add_days (tomorrow_begin, 1);

    struct {
        bool is_12h;
        GDateTime * now;
        GDateTime * then;
        GDateTime * then_end;
        const char * expected_format_string;
    } test_cases[] = {
        { false, g_date_time_ref(day), g_date_time_ref(day_begin), g_date_time_ref(day_end), _("Today") },
        { true, g_date_time_ref(day), g_date_time_ref(day_begin), g_date_time_ref(day_end), _("Today") },
        { false, g_date_time_ref(day), g_date_time_ref(tomorrow_begin), g_date_time_ref(tomorrow_end), _("Tomorrow") },
        { true, g_date_time_ref(day), g_date_time_ref(tomorrow_begin), g_date_time_ref(tomorrow_end), _("Tomorrow") }
    };

    for (int i=0, n=G_N_ELEMENTS(test_cases); i<n; i++)
    {
        if (test_cases[i].is_12h ? Set12hLocale() : Set24hLocale())
        {
            std::shared_ptr<MockClock> mock (new MockClock(test_cases[i].now));
            std::shared_ptr<Clock> clock = std::dynamic_pointer_cast<Clock>(mock);
            DesktopFormatter f (clock);
          
            std::string fmt = f.getRelativeFormat (test_cases[i].then, test_cases[i].then_end);
            ASSERT_STREQ (test_cases[i].expected_format_string, fmt.c_str());

            g_clear_pointer (&test_cases[i].now, g_date_time_unref);
            g_clear_pointer (&test_cases[i].then, g_date_time_unref);
            g_clear_pointer (&test_cases[i].then_end, g_date_time_unref);
        }
    }

    g_date_time_unref (tomorrow_end);
    g_date_time_unref (tomorrow_begin);
    g_date_time_unref (day_end);
    g_date_time_unref (day_begin);
    g_date_time_unref (day);
}


