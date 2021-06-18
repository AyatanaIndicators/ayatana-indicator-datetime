/*
 * Copyright 2015 Canonical Ltd.
 * Copyright 2021 Robert Tari
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
 *   Robert Tari <robert@tari.in>
 */

#include <datetime/date-time.h>

#include "glib-fixture.h"

using namespace ayatana::indicator::datetime;

/***
****
***/

class DateTimeFixture: public GlibFixture
{
  public:

    DateTimeFixture() =default;
    virtual ~DateTimeFixture() =default;

  private:

    typedef GlibFixture super;

  protected:

    GRand * m_rand = nullptr;

    virtual void SetUp() override
    {
      super::SetUp();

      m_rand = g_rand_new();
    }

    virtual void TearDown() override
    {
      g_clear_pointer(&m_rand, g_rand_free);

      super::TearDown();
    }

    DateTime random_day()
    {
        GTimeZone * universal = g_time_zone_new_utc();
        DateTime point(universal,
                       g_rand_int_range(m_rand, 1970, 3000),
                       g_rand_int_range(m_rand, 1, 13),
                       g_rand_int_range(m_rand, 1, 29),
                       g_rand_int_range(m_rand, 0, 24),
                       g_rand_int_range(m_rand, 0, 60),
                       g_rand_double_range(m_rand, 0, 60.0));
        g_time_zone_unref(universal);
        return point;
    }
};

/***
****
***/

TEST_F(DateTimeFixture, StartAndEnd)
{
    const int n_iterations{10000};

    for (int i{0}; i<n_iterations; ++i)
    {
        const auto day = random_day();
        int dayy{0}, daym{0}, dayd{0};
        day.ymd(dayy, daym, dayd);

        // test start-of-month
        auto test = day.start_of_month();
        int testy{0}, testm{0}, testd{0};
        test.ymd(testy, testm, testd);
        EXPECT_EQ(dayy, testy);
        EXPECT_EQ(daym, testm);
        EXPECT_EQ(1, testd);
        EXPECT_EQ(0, test.hour());
        EXPECT_EQ(0, test.minute());
        EXPECT_EQ(0, (int)test.seconds());

        // test start-of-day
        test = day.start_of_day();
        testy = -1;
        testm = -1;
        testd = -1;
        test.ymd(testy, testm, testd);
        EXPECT_EQ(dayy, testy);
        EXPECT_EQ(daym, testm);
        EXPECT_EQ(dayd, testd);
        EXPECT_EQ(0, test.hour());
        EXPECT_EQ(0, test.minute());
        EXPECT_EQ(0, (int)test.seconds());

        // test start-of-minute
        test = day.start_of_minute();
        testy = -1;
        testm = -1;
        testd = -1;
        test.ymd(testy, testm, testd);
        EXPECT_EQ(dayy, testy);
        EXPECT_EQ(daym, testm);
        EXPECT_EQ(dayd, testd);
        EXPECT_EQ(day.hour(), test.hour());
        EXPECT_EQ(day.minute(), test.minute());
        EXPECT_EQ(0, (int)test.seconds());

        // test end-of-day
        test = day.end_of_day();
        testy = -1;
        testm = -1;
        testd = -1;
        test.ymd(testy, testm, testd);
        EXPECT_EQ(dayy, testy);
        EXPECT_EQ(daym, testm);
        EXPECT_EQ(dayd, testd);
        EXPECT_EQ(day.add_days(1).start_of_day(), test.add_full(0,0,0,0,0,1));

        // test end-of-month
        test = day.end_of_month();
        testy = -1;
        testm = -1;
        testd = -1;
        test.ymd(testy, testm, testd);
        EXPECT_EQ(dayy, testy);
        EXPECT_EQ(daym, testm);
        EXPECT_EQ(day.add_days(31).start_of_month(), test.add_full(0,0,0,0,0,1));
    }
}

