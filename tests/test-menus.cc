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


#include "actions-mock.h"
#include "state-fixture.h"

#include <datetime/clock-mock.h>
#include <datetime/formatter.h>
#include <datetime/locations.h>
#include <datetime/menu.h>
#include <datetime/planner-mock.h>
#include <datetime/service.h>
#include <datetime/state.h>
#include <datetime/timezones.h>

#include <gio/gio.h>

using namespace unity::indicator::datetime;

class MenuFixture: public StateFixture
{
private:
    typedef StateFixture super;

protected:
    std::shared_ptr<MenuFactory> m_menu_factory;
    std::vector<std::shared_ptr<Menu>> m_menus;

    virtual void SetUp()
    {
        super::SetUp();

        // build the menus on top of the actions and state
        m_menu_factory.reset(new MenuFactory(m_actions, m_state));
        for(int i=0; i<Menu::NUM_PROFILES; i++)
            m_menus.push_back(m_menu_factory->buildMenu(Menu::Profile(i)));
    }

    virtual void TearDown()
    {
        m_menus.clear();
        m_menu_factory.reset();

        super::TearDown();
    }

    void InspectHeader(GMenuModel* menu_model, const std::string& name)
    {
        // check that there's a header menuitem
        EXPECT_EQ(1,g_menu_model_get_n_items(menu_model));
        gchar* str = nullptr;
        g_menu_model_get_item_attribute(menu_model, 0, "x-canonical-type", "s", &str);
        EXPECT_STREQ("com.canonical.indicator.root", str);
        g_clear_pointer(&str, g_free);
        g_menu_model_get_item_attribute(menu_model, 0, G_MENU_ATTRIBUTE_ACTION, "s", &str);
        const auto action_name = name + "-header";
        EXPECT_EQ(std::string("indicator.")+action_name, str);
        g_clear_pointer(&str, g_free);

        // check the header
        auto dict = g_action_group_get_action_state(m_actions->action_group(), action_name.c_str());
        EXPECT_TRUE(dict != nullptr);
        EXPECT_TRUE(g_variant_is_of_type(dict, G_VARIANT_TYPE_VARDICT));
        auto v = g_variant_lookup_value(dict, "accessible-desc", G_VARIANT_TYPE_STRING);
        EXPECT_TRUE(v != nullptr);
        g_variant_unref(v);
        v = g_variant_lookup_value(dict, "label", G_VARIANT_TYPE_STRING);
        EXPECT_TRUE(v != nullptr);
        g_variant_unref(v);
        v = g_variant_lookup_value(dict, "title", G_VARIANT_TYPE_STRING);
        EXPECT_TRUE(v != nullptr);
        g_variant_unref(v);
        v = g_variant_lookup_value(dict, "visible", G_VARIANT_TYPE_BOOLEAN);
        EXPECT_TRUE(v != nullptr);
        g_variant_unref(v);
        g_variant_unref(dict);
    }

    void InspectCalendar(GMenuModel* menu_model, Menu::Profile profile)
    {
        gchar* str = nullptr;
        const auto actions_expected = (profile == Menu::Desktop) || (profile == Menu::Phone);
        const auto calendar_expected = (profile == Menu::Desktop) || (profile == Menu::DesktopGreeter);

        // get the calendar section
        auto submenu = g_menu_model_get_item_link(menu_model, 0, G_MENU_LINK_SUBMENU);
        auto section = g_menu_model_get_item_link(submenu, Menu::Calendar, G_MENU_LINK_SECTION);

        // should be one or two items: a date label and maybe a calendar
        ASSERT_TRUE(section != nullptr);
        auto n_expected = calendar_expected ? 2 : 1;
        EXPECT_EQ(n_expected, g_menu_model_get_n_items(section));

        // look at the date menuitem
        g_menu_model_get_item_attribute(section, 0, G_MENU_ATTRIBUTE_LABEL, "s", &str);
        const auto now = m_state->clock->localtime();
        EXPECT_EQ(now.format("%A, %e %B %Y"), str);
      
        g_clear_pointer(&str, g_free);

        g_menu_model_get_item_attribute(section, 0, G_MENU_ATTRIBUTE_ACTION, "s", &str);
        if (actions_expected)
            EXPECT_STREQ("indicator.activate-planner", str);
        else
            EXPECT_TRUE(str == nullptr);
        g_clear_pointer(&str, g_free);

        // look at the calendar menuitem
        if (calendar_expected)
        {
            g_menu_model_get_item_attribute(section, 1, "x-canonical-type", "s", &str);
            EXPECT_STREQ("com.canonical.indicator.calendar", str);
            g_clear_pointer(&str, g_free);

            g_menu_model_get_item_attribute(section, 1, G_MENU_ATTRIBUTE_ACTION, "s", &str);
            EXPECT_STREQ("indicator.calendar", str);
            g_clear_pointer(&str, g_free);

            g_menu_model_get_item_attribute(section, 1, "activation-action", "s", &str);
            if (actions_expected)
                EXPECT_STREQ("indicator.activate-planner", str);
            else
                EXPECT_TRUE(str == nullptr);
            g_clear_pointer(&str, g_free);
        }

        g_clear_object(&section);

        // now change the clock and see if the date label changes appropriately

        auto gdt_tomorrow = g_date_time_add_days(now.get(), 1);
        auto tomorrow = DateTime(gdt_tomorrow);
        g_date_time_unref(gdt_tomorrow);
        m_clock->set_localtime(tomorrow);
        wait_msec();

        section = g_menu_model_get_item_link(submenu, Menu::Calendar, G_MENU_LINK_SECTION);
        g_menu_model_get_item_attribute(section, 0, G_MENU_ATTRIBUTE_LABEL, "s", &str);
        EXPECT_EQ(tomorrow.format("%A, %e %B %Y"), str);
        g_clear_pointer(&str, g_free);
        g_clear_object(&section);

        // cleanup
        g_object_unref(submenu);
    }

    void InspectAppointments(GMenuModel* menu_model, Menu::Profile profile)
    {
        const bool appointments_expected = (profile == Menu::Desktop)
                                        || (profile == Menu::Phone);

        // get the Appointments section
        auto submenu = g_menu_model_get_item_link(menu_model, 0, G_MENU_LINK_SUBMENU);

        // there shouldn't be any menuitems when "show events" is false
        m_state->settings->show_events.set(false);
        wait_msec();
        auto section = g_menu_model_get_item_link(submenu, Menu::Appointments, G_MENU_LINK_SECTION);
        EXPECT_EQ(0, g_menu_model_get_n_items(section));
        g_clear_object(&section);

        // when "show_events" is true,
        // there should be an "add event" button even if there aren't any appointments
        std::vector<Appointment> appointments;
        m_state->settings->show_events.set(true);
        m_state->planner->upcoming.set(appointments);
        wait_msec();
        section = g_menu_model_get_item_link(submenu, Menu::Appointments, G_MENU_LINK_SECTION);
        int expected_n = appointments_expected ? 1 : 0;
        EXPECT_EQ(expected_n, g_menu_model_get_n_items(section));
        g_clear_object(&section);

        // try adding a few appointments and see if the menu updates itself

        const auto now = m_state->clock->localtime();
        auto gdt_tomorrow = g_date_time_add_days(now.get(), 1);
        const auto tomorrow = DateTime(gdt_tomorrow);
        g_date_time_unref(gdt_tomorrow);

        Appointment a1; // an alarm clock appointment
        a1.color = "red";
        a1.summary = "Alarm";
        a1.summary = "http://www.example.com/";
        a1.uid = "example";
        a1.has_alarms = true;
        a1.begin = a1.end = tomorrow;
        appointments.push_back(a1);

        Appointment a2; // a non-alarm appointment
        a2.color = "green";
        a2.summary = "Other Text";
        a2.summary = "http://www.monkey.com/";
        a2.uid = "monkey";
        a2.has_alarms = false;
        a2.begin = a2.end = tomorrow;
        appointments.push_back(a2);

        m_state->planner->upcoming.set(appointments);
        wait_msec(); // wait a moment for the menu to update

        section = g_menu_model_get_item_link(submenu, Menu::Appointments, G_MENU_LINK_SECTION);
        expected_n = appointments_expected ? 3 : 0;
        EXPECT_EQ(expected_n, g_menu_model_get_n_items(section));
        if (appointments_expected)
        {
            gchar * str = nullptr;

            // test the alarm
            //  - confirm it has an x-canonical-type of "alarm"
            g_menu_model_get_item_attribute(section, 0, "x-canonical-type", "s", &str);
            EXPECT_STREQ("com.canonical.indicator.alarm", str);
            g_clear_pointer(&str, g_free);
            //  - confirm it has a nonempty x-canonical-time-format
            g_menu_model_get_item_attribute(section, 0, "x-canonical-time-format", "s", &str);
            EXPECT_TRUE(str && *str);
            g_clear_pointer(&str, g_free);
            //  - confirm it has a serialized icon attribute
            auto v = g_menu_model_get_item_attribute_value(section, 0, G_MENU_ATTRIBUTE_ICON, nullptr);
            EXPECT_TRUE(v != nullptr);
            auto icon = g_icon_deserialize(v);
            EXPECT_TRUE(icon != nullptr);
            g_clear_object(&icon);
            g_clear_pointer(&v, g_variant_unref);

            // test the appointment
            //  - confirm it has an x-canonical-type of "appointment"
            g_menu_model_get_item_attribute(section, 1, "x-canonical-type", "s", &str);
            EXPECT_STREQ("com.canonical.indicator.appointment", str);
            g_clear_pointer(&str, g_free);
            //  - confirm it has a nonempty x-canonical-time-format
            g_menu_model_get_item_attribute(section, 0, "x-canonical-time-format", "s", &str);
            EXPECT_TRUE(str && *str);
            g_clear_pointer(&str, g_free);
            //  - confirm its color matches the one we fed the appointments vector
            g_menu_model_get_item_attribute(section, 1, "x-canonical-color", "s", &str);
            EXPECT_EQ(a2.color, str);
            g_clear_pointer(&str, g_free);
        }
        g_clear_object(&section);

        g_object_unref(submenu);
    }

    void CompareLocationsTo(GMenuModel* menu_model, const std::vector<Location>& locations)
    {
        // get the Locations section
        auto submenu = g_menu_model_get_item_link(menu_model, 0, G_MENU_LINK_SUBMENU);
        auto section = g_menu_model_get_item_link(submenu, Menu::Locations, G_MENU_LINK_SECTION);

        // confirm that section's menuitems mirror the "locations" vector
        const auto n = locations.size();
        ASSERT_EQ(n, g_menu_model_get_n_items(section));
        for (guint i=0; i<n; i++)
        {
            gchar* str = nullptr;

            // confirm that the x-canonical-type is right
            g_menu_model_get_item_attribute(section, i, "x-canonical-type", "s", &str);
            EXPECT_STREQ("com.canonical.indicator.location", str);
            g_clear_pointer(&str, g_free);

            // confirm that the timezones match the ones in the vector
            g_menu_model_get_item_attribute(section, i, "x-canonical-timezone", "s", &str);
            EXPECT_EQ(locations[i].zone(), str);
            g_clear_pointer(&str, g_free);

            // confirm that x-canonical-time-format has some kind of time format string
            g_menu_model_get_item_attribute(section, i, "x-canonical-time-format", "s", &str);
            EXPECT_TRUE(str && *str && (strchr(str,'%')!=nullptr));
            g_clear_pointer(&str, g_free);
        }

        g_clear_object(&section);
        g_clear_object(&submenu);
    }

    void InspectLocations(GMenuModel* menu_model, Menu::Profile profile)
    {
        const bool locations_expected = profile == Menu::Desktop;

        // when there aren't any locations, confirm the menu is empty
        const std::vector<Location> empty;
        m_state->locations->locations.set(empty);
        wait_msec();
        CompareLocationsTo(menu_model, empty);

        // add some locations and confirm the menu picked up our changes
        Location l1 ("America/Chicago", "Dallas");
        Location l2 ("America/Arizona", "Phoenix");
        std::vector<Location> locations({l1, l2});
        m_state->locations->locations.set(locations);
        wait_msec();
        CompareLocationsTo(menu_model, locations_expected ? locations : empty);

        // now remove one of the locations...
        locations.pop_back();
        m_state->locations->locations.set(locations);
        wait_msec();
        CompareLocationsTo(menu_model, locations_expected ? locations : empty);
    }

    void InspectSettings(GMenuModel* menu_model, Menu::Profile profile)
    {
        std::string expected_action;

        if (profile == Menu::Desktop)
            expected_action = "indicator.activate-desktop-settings";
        else if (profile == Menu::Phone)
            expected_action = "indicator.activate-phone-settings";

        // get the Settings section
        auto submenu = g_menu_model_get_item_link(menu_model, 0, G_MENU_LINK_SUBMENU);
        auto section = g_menu_model_get_item_link(submenu, Menu::Settings, G_MENU_LINK_SECTION);

        if (expected_action.empty())
        {
            EXPECT_EQ(0, g_menu_model_get_n_items(section));
        }
        else
        {
            EXPECT_EQ(1, g_menu_model_get_n_items(section));
            gchar* str = nullptr;
            g_menu_model_get_item_attribute(section, 0, G_MENU_ATTRIBUTE_ACTION, "s", &str);
            EXPECT_EQ(expected_action, str);
            g_clear_pointer(&str, g_free);
        }

        g_clear_object(&section);
        g_object_unref(submenu);
    }
};


TEST_F(MenuFixture, HelloWorld)
{
    EXPECT_EQ(Menu::NUM_PROFILES, m_menus.size());
    for (int i=0; i<Menu::NUM_PROFILES; i++)
    {
        EXPECT_TRUE(m_menus[i] != false);
        EXPECT_TRUE(m_menus[i]->menu_model() != nullptr);
        EXPECT_EQ(i, m_menus[i]->profile());
    }
    EXPECT_EQ(m_menus[Menu::Desktop]->name(), "desktop");
}

TEST_F(MenuFixture, Header)
{
    for(auto& menu : m_menus)
      InspectHeader(menu->menu_model(), menu->name());
}

TEST_F(MenuFixture, Sections)
{
    for(auto& menu : m_menus)
    {
        // check that the header has a submenu
        auto menu_model = menu->menu_model();
        auto submenu = g_menu_model_get_item_link(menu_model, 0, G_MENU_LINK_SUBMENU);
        EXPECT_TRUE(submenu != nullptr);
        EXPECT_EQ(Menu::NUM_SECTIONS, g_menu_model_get_n_items(submenu));
        g_object_unref(submenu);
    }
}

TEST_F(MenuFixture, Calendar)
{
    for(auto& menu : m_menus)
      InspectCalendar(menu->menu_model(), menu->profile());
}

TEST_F(MenuFixture, Appointments)
{
    for(auto& menu : m_menus)
      InspectAppointments(menu->menu_model(), menu->profile());
}

TEST_F(MenuFixture, Locations)
{
    for(auto& menu : m_menus)
      InspectLocations(menu->menu_model(), menu->profile());
}

TEST_F(MenuFixture, Settings)
{
    for(auto& menu : m_menus)
      InspectSettings(menu->menu_model(), menu->profile());
}


