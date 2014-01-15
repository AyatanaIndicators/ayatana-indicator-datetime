

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
#include <datetime/locations.h>
#include <datetime/locations-settings.h>
#include <datetime/settings-shared.h>

#include <glib/gi18n.h>

#include <langinfo.h>
#include <locale.h>

using unity::indicator::datetime::Location;
using unity::indicator::datetime::Locations;
using unity::indicator::datetime::SettingsLocations;
using unity::indicator::datetime::Timezones;

/***
****
***/

class LocationsFixture: public GlibFixture
{
  private:

    typedef GlibFixture super;

  protected:

    GSettings * settings = nullptr;
    std::shared_ptr<Timezones> timezones;
    const std::string nyc = "America/New_York";
    const std::string chicago = "America/Chicago";

    virtual void SetUp()
    {
      super::SetUp();

      settings = g_settings_new(SETTINGS_INTERFACE);
      const gchar * location_strv[] = { "America/Los_Angeles Oakland", "America/Chicago Chicago", "America/Chicago Oklahoma City", "America/Toronto Toronto", "Europe/London London", "Europe/Berlin Berlin", NULL };
      g_settings_set_strv(settings, SETTINGS_LOCATIONS_S, location_strv);
      g_settings_set_boolean(settings, SETTINGS_SHOW_LOCATIONS_S, true);

      timezones.reset(new Timezones);
      timezones->timezone.set(chicago);
      timezones->timezones.set(std::set<std::string>({ nyc, chicago }));
    }

    virtual void TearDown()
    {
      //timezones.reset(nullptr);
      g_clear_object(&settings);

      super::TearDown();
    }
};

TEST_F(LocationsFixture, Timezones)
{
    g_settings_set_boolean(settings, SETTINGS_SHOW_LOCATIONS_S, false);

    SettingsLocations locations(SETTINGS_INTERFACE, timezones);
    std::vector<Location> l = locations.locations.get();
    EXPECT_EQ(2, l.size());
    EXPECT_EQ("Chicago", l[0].name);
    EXPECT_EQ(chicago, l[0].zone);
    EXPECT_EQ("New York", l[1].name);
    EXPECT_EQ(nyc, l[1].zone);
}

TEST_F(LocationsFixture, SettingsLocations)
{
    SettingsLocations locations(SETTINGS_INTERFACE, timezones);

    std::vector<Location> l = locations.locations.get();
    EXPECT_EQ(7, l.size());
    EXPECT_EQ("Chicago", l[0].name);
    EXPECT_EQ(chicago, l[0].zone);
    EXPECT_EQ("New York", l[1].name);
    EXPECT_EQ(nyc, l[1].zone);
    EXPECT_EQ("Oakland", l[2].name);
    EXPECT_EQ("America/Los_Angeles", l[2].zone);
    EXPECT_EQ("Oklahoma City", l[3].name);
    EXPECT_EQ("America/Chicago", l[3].zone);
    EXPECT_EQ("Toronto", l[4].name);
    EXPECT_EQ("America/Toronto", l[4].zone);
    EXPECT_EQ("London", l[5].name);
    EXPECT_EQ("Europe/London", l[5].zone);
    EXPECT_EQ("Berlin", l[6].name);
    EXPECT_EQ("Europe/Berlin", l[6].zone);
}

TEST_F(LocationsFixture, ChangeLocationStrings)
{
    SettingsLocations locations(SETTINGS_INTERFACE, timezones);

    bool locations_changed = false;
    locations.locations.changed().connect([&locations_changed, this](const std::vector<Location>&){
                    locations_changed = true;
                    g_main_loop_quit(loop);
                });

    g_idle_add([](gpointer gsettings){
                    const gchar * strv[] = { "America/Los_Angeles Oakland", "Europe/London London", "Europe/Berlin Berlin", NULL };
                    g_settings_set_strv(static_cast<GSettings*>(gsettings), SETTINGS_LOCATIONS_S, strv);
                    return G_SOURCE_REMOVE;
                }, settings);

    g_main_loop_run(loop);

    EXPECT_TRUE(locations_changed);
    std::vector<Location> l = locations.locations.get();
    EXPECT_EQ(5, l.size());
    EXPECT_EQ("Chicago", l[0].name);
    EXPECT_EQ(chicago, l[0].zone);
    EXPECT_EQ("New York", l[1].name);
    EXPECT_EQ(nyc, l[1].zone);
    EXPECT_EQ("Oakland", l[2].name);
    EXPECT_EQ("America/Los_Angeles", l[2].zone);
    EXPECT_EQ("London", l[3].name);
    EXPECT_EQ("Europe/London", l[3].zone);
    EXPECT_EQ("Berlin", l[4].name);
    EXPECT_EQ("Europe/Berlin", l[4].zone);
    locations_changed = false;
}

TEST_F(LocationsFixture, ChangeLocationVisibility)
{
    SettingsLocations locations(SETTINGS_INTERFACE, timezones);

    bool locations_changed = false;
    locations.locations.changed().connect([&locations_changed, this](const std::vector<Location>&){
                    locations_changed = true;
                    g_main_loop_quit(loop);
                });

    g_idle_add([](gpointer gsettings){
                    g_settings_set_boolean(static_cast<GSettings*>(gsettings), SETTINGS_SHOW_LOCATIONS_S, false);
                    return G_SOURCE_REMOVE;
                }, settings);

    g_main_loop_run(loop);

    EXPECT_TRUE(locations_changed);
    std::vector<Location> l = locations.locations.get();
    EXPECT_EQ(2, l.size());
    EXPECT_EQ("Chicago", l[0].name);
    EXPECT_EQ(chicago, l[0].zone);
    EXPECT_EQ("New York", l[1].name);
    EXPECT_EQ(nyc, l[1].zone);
}
