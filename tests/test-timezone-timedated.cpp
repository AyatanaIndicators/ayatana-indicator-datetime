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

#include "timedated-fixture.h"

#include <datetime/timezone-timedated.h>

using namespace ayatana::indicator::datetime;

using TestTimedatedFixture = TimedatedFixture;

/***
****
***/

TEST_F(TestTimedatedFixture, HelloWorld)
{
}

/**
 * Test that the tzid is right if timedated isn't available
 */
TEST_F(TestTimedatedFixture, DefaultTimezone)
{
    const std::string expected_tzid{"Etc/Utc"};

    TimedatedTimezone tmp;
    EXPECT_TZID(expected_tzid, tmp);
}

/**
 * Test that the tzid is right if timedated shows BEFORE we start
 */
TEST_F(TestTimedatedFixture, Timedate1First)
{
    const std::string expected_tzid{"America/Chicago"};

    start_timedate1(expected_tzid);
    TimedatedTimezone tmp;
    EXPECT_TZID(expected_tzid, tmp);
}

/**
 * Test that the tzid is right if timedated shows AFTER we start
 */
TEST_F(TestTimedatedFixture, Timedate1Last)
{
    const std::string expected_tzid("America/Los_Angeles");

    TimedatedTimezone tmp;
    start_timedate1(expected_tzid);
    EXPECT_TZID(expected_tzid, tmp);
}

/**
 * Test that the tzid is right if timedated's property changes
 */
TEST_F(TestTimedatedFixture, TimezoneChange)
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
