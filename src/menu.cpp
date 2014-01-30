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

#include <datetime/menu.h>

#include <datetime/formatter.h>
#include <datetime/state.h>

#include <json-glib/json-glib.h>

#include <glib/gi18n.h>
#include <gio/gio.h>

namespace unity {
namespace indicator {
namespace datetime {

/****
*****
****/

Menu::Menu (Profile profile_in, const std::string& name_in):
    m_profile(profile_in),
    m_name(name_in)
{
}

const std::string& Menu::name() const
{
    return m_name;
}

Menu::Profile Menu::profile() const
{
    return m_profile;
}

GMenuModel* Menu::menu_model()
{
    return G_MENU_MODEL(m_menu);
}


/****
*****
****/


#define FALLBACK_ALARM_CLOCK_ICON_NAME "clock"
#define CALENDAR_ICON_NAME "calendar"

class MenuImpl: public Menu
{
protected:
    MenuImpl(const Menu::Profile profile_in,
             const std::string& name_in,
             std::shared_ptr<State>& state,
             std::shared_ptr<Actions>& actions,
             std::shared_ptr<Formatter> formatter):
        Menu(profile_in, name_in),
        m_state(state),
        m_actions(actions),
        m_formatter(formatter)
    {
        // preload the alarm icon from click
        m_serialized_alarm_icon = create_alarm_icon();

        // initialize the menu
        create_gmenu();
        for (int i=0; i<NUM_SECTIONS; i++)
            update_section(Section(i));

        // listen for state changes so we can update the menu accordingly
        m_formatter->header.changed().connect([this](const std::string&){
            update_header();
        });
        m_formatter->headerFormat.changed().connect([this](const std::string&){
            update_section(Locations); // need to update x-canonical-time-format
        });
        m_formatter->relativeFormatChanged.connect([this](){
            update_section(Appointments); // uses formatter.getRelativeFormat()
            update_section(Locations); // uses formatter.getRelativeFormat()
        });
        m_state->settings->show_clock.changed().connect([this](bool){
            update_header(); // update header's label
            update_section(Locations); // locations' relative time may have changed
        });
        m_state->settings->show_calendar.changed().connect([this](bool){
            update_section(Calendar);
        });
        m_state->settings->show_events.changed().connect([this](bool){
            update_section(Appointments); // showing events got toggled
        });
        m_state->planner->upcoming.changed().connect([this](const std::vector<Appointment>&){
            update_section(Appointments); // "upcoming" is the list of Appointments we show
        });
        m_state->clock->dateChanged.connect([this](){
            update_section(Calendar); // need to update the Date menuitem
            update_section(Locations); // locations' relative time may have changed
        });
        m_state->locations->locations.changed().connect([this](const std::vector<Location>&) {
            update_section(Locations); // "locations" is the list of Locations we show
        });
    }

    virtual ~MenuImpl()
    {
        g_clear_object(&m_menu);
        g_clear_pointer(&m_serialized_alarm_icon, g_variant_unref);
        g_clear_pointer(&m_serialized_calendar_icon, g_variant_unref);
    }

    virtual GVariant* create_header_state() =0;

    void update_header()
    {
        auto action_group = m_actions->action_group();
        auto action_name = name() + "-header";
        auto state = create_header_state();
        g_action_group_change_action_state(action_group, action_name.c_str(), state);
    }

    std::shared_ptr<State> m_state;
    std::shared_ptr<Actions> m_actions;
    std::shared_ptr<Formatter> m_formatter;
    GMenu* m_submenu = nullptr;

    GVariant* get_serialized_alarm_icon() { return m_serialized_alarm_icon; }

private:

    /* try to get the clock app's filename from click. (/$pkgdir/$icon) */
    static GVariant* create_alarm_icon()
    {
        GVariant* serialized = nullptr;
        gchar* icon_filename = nullptr;
        gchar* standard_error = nullptr;
        gchar* pkgdir = nullptr;

        g_spawn_command_line_sync("click pkgdir com.ubuntu.clock", &pkgdir, &standard_error, nullptr, nullptr);
        g_clear_pointer(&standard_error, g_free);
        if (pkgdir != nullptr)
        {
            gchar* manifest = nullptr;
            g_strstrip(pkgdir);
            g_spawn_command_line_sync("click info com.ubuntu.clock", &manifest, &standard_error, nullptr, nullptr);
            g_clear_pointer(&standard_error, g_free);
            if (manifest != nullptr)
            {
                JsonParser* parser = json_parser_new();
                if (json_parser_load_from_data(parser, manifest, -1, nullptr))
                {
                    JsonNode* root = json_parser_get_root(parser); /* transfer-none */
                    if ((root != nullptr) && (JSON_NODE_TYPE(root) == JSON_NODE_OBJECT))
                    {
                        JsonObject* o = json_node_get_object(root); /* transfer-none */
                        const gchar* icon_name = json_object_get_string_member(o, "icon");
                        if (icon_name != nullptr)
                            icon_filename = g_build_filename(pkgdir, icon_name, nullptr);
                    }
                }
                g_object_unref(parser);
                g_free(manifest);
            }
            g_free(pkgdir);
        }

        if (icon_filename != nullptr)
        {
            GFile* file = g_file_new_for_path(icon_filename);
            GIcon* icon = g_file_icon_new(file);

            serialized = g_icon_serialize(icon);

            g_object_unref(icon);
            g_object_unref(file);
            g_free(icon_filename);
        }

        if (serialized == nullptr)
        {
            auto i = g_themed_icon_new_with_default_fallbacks(FALLBACK_ALARM_CLOCK_ICON_NAME);
            serialized = g_icon_serialize(i);
            g_object_unref(i);
        }

        return serialized;
    }

    GVariant* get_serialized_calendar_icon()
    {
        if (G_UNLIKELY(m_serialized_calendar_icon == nullptr))
        {
            auto i = g_themed_icon_new_with_default_fallbacks(CALENDAR_ICON_NAME);
            m_serialized_calendar_icon = g_icon_serialize(i);
            g_object_unref(i);
        }

        return m_serialized_calendar_icon;
    }

    void create_gmenu()
    {
        g_assert(m_submenu == nullptr);

        m_submenu = g_menu_new();

        // build placeholders for the sections
        for(int i=0; i<NUM_SECTIONS; i++)
        {
            GMenuItem * item = g_menu_item_new(nullptr, nullptr);
            g_menu_append_item(m_submenu, item);
            g_object_unref(item);
        }

        // add submenu to the header
        const auto detailed_action = std::string("indicator.") + name() + "-header";
        auto header = g_menu_item_new(nullptr, detailed_action.c_str());
        g_menu_item_set_attribute(header, "x-canonical-type", "s",
                                  "com.canonical.indicator.root");
        g_menu_item_set_attribute(header, "submenu-action", "s",
                                  "indicator.calendar-active");
        g_menu_item_set_submenu(header, G_MENU_MODEL(m_submenu));
        g_object_unref(m_submenu);

        // add header to the menu
        m_menu = g_menu_new();
        g_menu_append_item(m_menu, header);
        g_object_unref(header);
    }

    GMenuModel* create_calendar_section(Profile profile)
    {
        const bool allow_activation = (profile == Desktop)
                                   || (profile == Phone);
        const bool show_calendar = m_state->settings->show_calendar.get() &&
                                   ((profile == Desktop) || (profile == DesktopGreeter));
        auto menu = g_menu_new();

        // add a menuitem that shows the current date
        auto label = m_state->clock->localtime().format(_("%A, %e %B %Y"));
        auto item = g_menu_item_new (label.c_str(), nullptr);
        auto v = get_serialized_calendar_icon();
        g_menu_item_set_attribute_value (item, G_MENU_ATTRIBUTE_ICON, v);
        if (allow_activation)
        {
            v = g_variant_new_int64(0);
            const char* action = "indicator.activate-planner";
            g_menu_item_set_action_and_target_value (item, action, v);
        }
        g_menu_append_item(menu, item);
        g_object_unref(item);

        // add calendar
        if (show_calendar)
        {
            item = g_menu_item_new ("[calendar]", NULL);
            v = g_variant_new_int64(0);
            g_menu_item_set_action_and_target_value (item, "indicator.calendar", v);
            g_menu_item_set_attribute (item, "x-canonical-type",
                                       "s", "com.canonical.indicator.calendar");
            if (allow_activation)
            {
                g_menu_item_set_attribute (item, "activation-action",
                                           "s", "indicator.activate-planner");
            }
            g_menu_append_item (menu, item);
            g_object_unref (item);
        }

        return G_MENU_MODEL(menu);
    }

    void add_appointments(GMenu* menu, Profile profile)
    {
        int n = 0;
        const int MAX_APPTS = 5;
        std::set<std::string> added;

        for (const auto& appt : m_state->planner->upcoming.get())
        {
            if (n++ >= MAX_APPTS)
                break;

            if (added.count(appt.uid))
                continue;

            added.insert(appt.uid);

            GDateTime* begin = appt.begin();
            GDateTime* end = appt.end();
            auto fmt = m_formatter->getRelativeFormat(begin, end);
            auto unix_time = g_date_time_to_unix(begin);

            auto menu_item = g_menu_item_new (appt.summary.c_str(), nullptr);
            g_menu_item_set_attribute (menu_item, "x-canonical-time", "x", unix_time);
            g_menu_item_set_attribute (menu_item, "x-canonical-time-format", "s", fmt.c_str());

            if (appt.has_alarms)
            {
                g_menu_item_set_attribute (menu_item, "x-canonical-type", "s", "com.canonical.indicator.alarm");
                g_menu_item_set_attribute_value(menu_item, G_MENU_ATTRIBUTE_ICON, get_serialized_alarm_icon());
            }
            else
            {
                g_menu_item_set_attribute (menu_item, "x-canonical-type", "s", "com.canonical.indicator.appointment");
            }

            if (!appt.color.empty())
                g_menu_item_set_attribute (menu_item, "x-canonical-color", "s", appt.color.c_str());
     
            if (profile == Phone)
                g_menu_item_set_action_and_target_value (menu_item,
                                                     "indicator.activate-appointment",
                                                     g_variant_new_string (appt.uid.c_str()));
            else
                g_menu_item_set_action_and_target_value (menu_item,
                                                         "indicator.activate-planner",
                                                         g_variant_new_int64 (unix_time));
            g_menu_append_item (menu, menu_item);
            g_object_unref (menu_item);
        }
    }

    GMenuModel* create_appointments_section(Profile profile)
    {
        auto menu = g_menu_new();

        if ((profile==Desktop) && m_state->settings->show_events.get())
        {
            add_appointments (menu, profile);

            // add the 'Add Event…' menuitem
            auto menu_item = g_menu_item_new(_("Add Event…"), nullptr);
            const gchar* action_name = "indicator.activate-planner";
            auto v = g_variant_new_int64(0);
            g_menu_item_set_action_and_target_value(menu_item, action_name, v);
            g_menu_append_item(menu, menu_item);
            g_object_unref(menu_item);
        }
        else if (profile==Phone)
        {
            auto menu_item = g_menu_item_new (_("Clock"), "indicator.activate-phone-clock-app");
            g_menu_item_set_attribute_value (menu_item, G_MENU_ATTRIBUTE_ICON, get_serialized_alarm_icon());
            g_menu_append_item (menu, menu_item);
            g_object_unref (menu_item);

            add_appointments (menu, profile);
        }

        return G_MENU_MODEL(menu);
    }

    GMenuModel* create_locations_section(Profile profile)
    {
        GMenu* menu = g_menu_new();

        if (profile == Desktop)
        {
            const auto now = m_state->clock->localtime();

            for(const auto& location : m_state->locations->locations.get())
            {
                const auto& zone = location.zone();
                const auto& name = location.name();
                const auto zone_now = now.to_timezone(zone);
                const auto fmt = m_formatter->getRelativeFormat(zone_now.get());
                auto detailed_action = g_strdup_printf("indicator.set-location::%s %s", zone.c_str(), name.c_str());
                auto i = g_menu_item_new (name.c_str(), detailed_action);
                g_menu_item_set_attribute(i, "x-canonical-type", "s", "com.canonical.indicator.location");
                g_menu_item_set_attribute(i, "x-canonical-timezone", "s", zone.c_str());
                g_menu_item_set_attribute(i, "x-canonical-time-format", "s", fmt.c_str());
                g_menu_append_item (menu, i);
                g_object_unref(i);
                g_free(detailed_action);
            }
        }

        return G_MENU_MODEL(menu);
    }

    GMenuModel* create_settings_section(Profile profile)
    {
        auto menu = g_menu_new();

        if (profile == Desktop)
        {
            g_menu_append (menu, _("Date & Time Settings…"), "indicator.activate-desktop-settings");
        }
        else if (profile == Phone)
        {
            g_menu_append (menu, _("Time & Date settings…"), "indicator.activate-phone-settings");
        }

        return G_MENU_MODEL (menu);
    }

    void update_section(Section section)
    {
        GMenuModel * model;
        const auto p = profile();

        switch (section)
        {
            case Calendar: model = create_calendar_section(p); break;
            case Appointments: model = create_appointments_section(p); break;
            case Locations: model = create_locations_section(p); break;
            case Settings: model = create_settings_section(p); break;
            default: model = nullptr; g_warn_if_reached();
        }

        if (model)
        {
            g_menu_remove(m_submenu, section);
            g_menu_insert_section(m_submenu, section, nullptr, model);
            g_object_unref(model);
        }
    }

//private:
    GVariant * m_serialized_alarm_icon = nullptr;
    GVariant * m_serialized_calendar_icon = nullptr;

}; // class MenuImpl



/***
****
***/

class DesktopBaseMenu: public MenuImpl
{
protected:
    DesktopBaseMenu(Menu::Profile profile_,
                    const std::string& name_,
                    std::shared_ptr<State>& state_,
                    std::shared_ptr<Actions>& actions_):
        MenuImpl(profile_, name_, state_, actions_,
                 std::shared_ptr<Formatter>(new DesktopFormatter(state_->clock, state_->settings)))
    {
        update_header();
    }

    GVariant* create_header_state()
    {
        const auto visible = m_state->settings->show_clock.get();
        const auto title = _("Date and Time");
        auto label = g_variant_new_string(m_formatter->header.get().c_str());

        GVariantBuilder b;
        g_variant_builder_init(&b, G_VARIANT_TYPE_VARDICT);
        g_variant_builder_add(&b, "{sv}", "accessible-desc", label);
        g_variant_builder_add(&b, "{sv}", "label", label);
        g_variant_builder_add(&b, "{sv}", "title", g_variant_new_string(title));
        g_variant_builder_add(&b, "{sv}", "visible", g_variant_new_boolean(visible));
        return g_variant_builder_end(&b);
    }
};

class DesktopMenu: public DesktopBaseMenu
{
public:
    DesktopMenu(std::shared_ptr<State>& state_, std::shared_ptr<Actions>& actions_):
        DesktopBaseMenu(Desktop,"desktop", state_, actions_) {}
};

class DesktopGreeterMenu: public DesktopBaseMenu
{
public:
    DesktopGreeterMenu(std::shared_ptr<State>& state_, std::shared_ptr<Actions>& actions_):
        DesktopBaseMenu(DesktopGreeter,"desktop_greeter", state_, actions_) {}
};

class PhoneBaseMenu: public MenuImpl
{
protected:
    PhoneBaseMenu(Menu::Profile profile_,
                  const std::string& name_,
                  std::shared_ptr<State>& state_,
                  std::shared_ptr<Actions>& actions_):
        MenuImpl(profile_, name_, state_, actions_,
                 std::shared_ptr<Formatter>(new PhoneFormatter(state_->clock)))
    {
        update_header();
    }

    GVariant* create_header_state()
    {
        // are there alarms?
        bool has_alarms = false;
        for(const auto& appointment : m_state->planner->upcoming.get())
            if((has_alarms = appointment.has_alarms))
                break;

        GVariantBuilder b;
        g_variant_builder_init(&b, G_VARIANT_TYPE_VARDICT);
        g_variant_builder_add(&b, "{sv}", "title", g_variant_new_string (_("Upcoming")));
        g_variant_builder_add(&b, "{sv}", "visible", g_variant_new_boolean (TRUE));
        if (has_alarms)
        {
            auto label = m_formatter->header.get();
            auto a11y = g_strdup_printf(_("%s (has alarms)"), label.c_str());
            g_variant_builder_add(&b, "{sv}", "label", g_variant_new_string(label.c_str()));
            g_variant_builder_add(&b, "{sv}", "accessible-desc", g_variant_new_take_string(a11y));
            g_variant_builder_add(&b, "{sv}", "icon", get_serialized_alarm_icon());
        }
        else
        {
            auto v = g_variant_new_string(m_formatter->header.get().c_str());
            g_variant_builder_add(&b, "{sv}", "label", v);
            g_variant_builder_add(&b, "{sv}", "accessible-desc", v);
        }
        return g_variant_builder_end (&b);
    }
};

class PhoneMenu: public PhoneBaseMenu
{
public:
    PhoneMenu(std::shared_ptr<State>& state_,
              std::shared_ptr<Actions>& actions_):
        PhoneBaseMenu(Phone, "phone", state_, actions_) {}
};

class PhoneGreeterMenu: public PhoneBaseMenu
{
public:
    PhoneGreeterMenu(std::shared_ptr<State>& state_,
                     std::shared_ptr<Actions>& actions_):
        PhoneBaseMenu(PhoneGreeter, "phone_greeter", state_, actions_) {}
};

/****
*****
****/

MenuFactory::MenuFactory(std::shared_ptr<Actions>& actions_,
                         std::shared_ptr<State>& state_):
    m_actions(actions_),
    m_state(state_)
{
}

std::shared_ptr<State> MenuFactory::state()
{
    return m_state;
}

std::shared_ptr<Menu>
MenuFactory::buildMenu(Menu::Profile profile)
{
    std::shared_ptr<Menu> menu;

    switch (profile)
    {
    case Menu::Desktop:
        menu.reset(new DesktopMenu(m_state, m_actions));
        break;

    case Menu::DesktopGreeter:
        menu.reset(new DesktopGreeterMenu(m_state, m_actions));
        break;

    case Menu::Phone:
        menu.reset(new PhoneMenu(m_state, m_actions));
        break;

    case Menu::PhoneGreeter:
        menu.reset(new PhoneGreeterMenu(m_state, m_actions));
        break;

    default:
        g_warn_if_reached();
        break;
    }
    
    return menu;
}

/****
*****
****/

} // namespace datetime
} // namespace indicator
} // namespace unity
