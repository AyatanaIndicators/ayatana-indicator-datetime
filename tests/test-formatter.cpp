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

#include <datetime/clock-mock.h>
#include <datetime/formatter.h>
#include <datetime/settings.h>

#include <glib/gi18n.h>

#include <langinfo.h>
#include <locale.h>

using namespace ayatana::indicator::datetime;

/***
****
***/

class FormatterFixture: public GlibFixture
{
  private:

    typedef GlibFixture super;
    gchar* m_original_locale = nullptr;

  protected:

    std::shared_ptr<Settings> m_settings;

    void SetUp() override
    {
      super::SetUp();

      m_settings.reset(new Settings);
      m_original_locale = g_strdup(setlocale(LC_TIME, nullptr));
    }

    void TearDown() override
    {
      m_settings.reset();

      setlocale(LC_TIME, m_original_locale);
      g_clear_pointer(&m_original_locale, g_free);

      super::TearDown();
    }

    bool SetLocale(const char* expected_locale, const char* name)
    {
      if (setlocale(LC_TIME, expected_locale) != nullptr)
        {
          return true;
        }
      else
        {
          g_message("Unable to set locale to %s; skipping %s locale tests. (Current LC_TIME: %s)",
                    expected_locale, name, setlocale(LC_TIME, nullptr));
          return false;
        }
    }

    inline bool Set24hLocale() { return SetLocale("C",           "24h"); }
    inline bool Set12hLocale() { return SetLocale("en_US.UTF-8", "12h"); }
};


/**
 * Test the phone header format
 */
TEST_F(FormatterFixture, DISABLED_TestPhoneHeader)
{
    auto now = DateTime::Local(2020, 10, 31, 18, 30, 59);
    auto clock = std::make_shared<MockClock>(now);

    // test the default value in a 24h locale
    if(Set24hLocale())
    {
        PhoneFormatter formatter(clock);
        EXPECT_EQ(std::string("%H:%M"), formatter.header_format.get());
        EXPECT_EQ(std::string("18:30"), formatter.header.get());
    }

    // test the default value in a 12h locale
    if(Set12hLocale())
    {
        PhoneFormatter formatter(clock);
        EXPECT_EQ(std::string("%l:%M %p"), formatter.header_format.get());
        EXPECT_EQ(std::string(" 6:30 PM"), formatter.header.get());
    }
}

#define EM_SPACE "\u2003"

/**
 * Test the default values of the desktop header format
 */
TEST_F(FormatterFixture, DISABLED_TestDesktopHeader)
{
  struct {
    bool is_12h;
    bool show_day;
    bool show_date;
    bool show_year;
    const char* expected_format_string;
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

  auto now = DateTime::Local(2020, 10, 31, 18, 30, 59);
  auto clock = std::make_shared<MockClock>(now);

  bool locale_set = false;
  for(const auto& test_case : test_cases)
    {
      test_case.is_12h ? locale_set = Set12hLocale() : locale_set = Set24hLocale();
      DesktopFormatter f(clock, m_settings);

      m_settings->show_day.set(test_case.show_day);
      m_settings->show_date.set(test_case.show_date);
      m_settings->show_year.set(test_case.show_year);

      if (locale_set)
        {
          ASSERT_STREQ(test_case.expected_format_string, f.header_format.get().c_str());
        }
      else
        {
          g_message("Ignoring test (expected: %s, probably flawed test result: %s).", test_case.expected_format_string, f.header_format.get().c_str());
        }
    }
}

/**
 * Test the default values of the desktop header format
 */
TEST_F(FormatterFixture, TestUpcomingTimes)
{
    auto a = DateTime::Local(2020, 10, 31, 18, 30, 59);

    struct {
        gboolean is_12h;
        DateTime now;
        DateTime then;
        const char* expected_format_string;
    } test_cases[] = {
        { true, a, a, "%l:%M %p" }, // identical time
        { true, a, a.add_full(0,0,0,1,0,0), "%l:%M %p" }, // later today
        { true, a, a.add_days(1), "Tomorrow" EM_SPACE "%l:%M %p" }, // tomorrow
        { true, a, a.add_days(2), "%a" EM_SPACE "%l:%M %p" },
        { true, a, a.add_days(6), "%a" EM_SPACE "%l:%M %p" },
        { true, a, a.add_days(7), "%a %d %b" EM_SPACE "%l:%M %p" }, // over one week away

        { false, a, a, "%H:%M" }, // identical time
        { false, a, a.add_full(0,0,0,1,0,0), "%H:%M" }, // later today
        { false, a, a.add_days(1), "Tomorrow" EM_SPACE "%H:%M" }, // tomorrow
        { false, a, a.add_days(2), "%a" EM_SPACE "%H:%M" },
        { false, a, a.add_days(6), "%a" EM_SPACE "%H:%M" },
        { false, a, a.add_days(7), "%a %d %b" EM_SPACE "%H:%M" } // over one week away
    };

    for(const auto& test_case : test_cases)
    {
        if (test_case.is_12h ? Set12hLocale() : Set24hLocale())
        {
            auto clock = std::make_shared<MockClock>(test_case.now);
            DesktopFormatter f(clock, m_settings);

            const auto fmt = f.relative_format(test_case.then.get());
            ASSERT_EQ(test_case.expected_format_string, fmt);
        }
    }
}


/**
 * Test the default values of the desktop header format
 */
TEST_F(FormatterFixture, TestEventTimes)
{
    auto day            = DateTime::Local(2013, 1, 1, 13, 0, 0);
    auto day_begin      = DateTime::Local(2013, 1, 1, 13, 0, 0);
    auto day_end        = day_begin.add_days(1);
    auto tomorrow_begin = day_begin.add_days(1);
    auto tomorrow_end   = tomorrow_begin.add_days(1);

    struct {
        bool is_12h;
        DateTime now;
        DateTime then;
        DateTime then_end;
        const char* expected_format_string;
    } test_cases[] = {
        { false, day, day_begin, day_end, _("Today") },
        { true, day, day_begin, day_end, _("Today") },
        { false, day, tomorrow_begin, tomorrow_end, _("Tomorrow") },
        { true, day, tomorrow_begin, tomorrow_end, _("Tomorrow") }
    };

    for(const auto& test_case : test_cases)
    {
        if (test_case.is_12h ? Set12hLocale() : Set24hLocale())
        {
            auto clock = std::make_shared<MockClock>(test_case.now);
            DesktopFormatter f(clock, m_settings);

            const auto fmt = f.relative_format(test_case.then.get(), test_case.then_end.get());
            ASSERT_STREQ(test_case.expected_format_string, fmt.c_str());
        }
    }
}
