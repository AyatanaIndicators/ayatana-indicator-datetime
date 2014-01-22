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

#include <datetime/utils.h>

#include <gtest/gtest.h>

TEST(UtilsTest, SplitSettingsLocation)
{
    struct {
        const char* location;
        const char* expected_zone;
        const char* expected_name;
    } test_cases[] = {
        { "America/Chicago Chicago", "America/Chicago", "Chicago" },
        { "America/Chicago Oklahoma City", "America/Chicago", "Oklahoma City" },
        { "America/Los_Angeles",      "America/Los_Angeles", "Los Angeles" },
        { "America/Los_Angeles  ",    "America/Los_Angeles", "Los Angeles" },
        { "  America/Los_Angeles",    "America/Los_Angeles", "Los Angeles" },
        { "  America/Los_Angeles   ", "America/Los_Angeles", "Los Angeles" },
        { "UTC UTC", "UTC", "UTC" }
    };

    for(const auto& test_case : test_cases)
    {
        char * zone = nullptr;
        char * name = nullptr;

        split_settings_location(test_case.location, &zone, &name);
        ASSERT_STREQ(test_case.expected_zone, zone);
        ASSERT_STREQ(test_case.expected_name, name);

        g_free(zone);
        g_free(name);
    }
}

TEST(UtilsTest, BeautifulTimezoneName)
{
    struct {
        const char* timezone;
        const char* location;
        const char* expected_name;
    } test_cases[] = {
        { "America/Chicago", NULL, "Chicago" },
        { "America/Chicago", "America/Chicago", "Chicago" },
        { "America/Chicago", "America/Chigago Chicago", "Chicago" },
        { "America/Chicago", "America/Chicago Oklahoma City", "Oklahoma City" },
        { "America/Chicago", "Europe/London London", "Chicago" }
    };

    for(const auto& test_case : test_cases)
    {
        auto name = get_beautified_timezone_name(test_case.timezone, test_case.location);
        EXPECT_STREQ(test_case.expected_name, name);
        g_free(name);
    }
}
