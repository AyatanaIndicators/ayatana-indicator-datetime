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

#ifndef INDICATOR_DATETIME_MENU_H
#define INDICATOR_DATETIME_MENU_H

#include <datetime/actions.h>
#include <datetime/state.h>

#include <memory> // std::shared_ptr
#include <vector>

#include <gio/gio.h> // GMenuModel

namespace unity {
namespace indicator {
namespace datetime {

/**
 * \brief A menu for a specific profile; eg, Desktop or Phone.
 *
 * @see MenuFactory
 */
class Menu
{
public:
    enum Profile { Desktop, DesktopGreeter, Phone, PhoneGreeter, NUM_PROFILES };
    enum Section { Calendar, Appointments, Locations, Settings, NUM_SECTIONS };
    const std::string& name() const { return m_name; }
    Profile profile() const { return m_profile; }
    GMenuModel* menu_model() { return G_MENU_MODEL(m_menu); }

protected:
    Menu (Profile profile_in, const std::string& name_in): m_profile(profile_in), m_name(name_in) {}
    virtual ~Menu() =default;
    GMenu* m_menu = nullptr;

private:
    const Profile m_profile;
    const std::string m_name;

    // we've got raw pointers in here, so disable copying
    Menu(const Menu&) =delete;
    Menu& operator=(const Menu&) =delete;
};

/**
 * \brief Builds a Menu for a given state and profile
 */
class MenuFactory
{
public:
    MenuFactory (std::shared_ptr<Actions>& actions, std::shared_ptr<State>& state);
    std::shared_ptr<Menu> buildMenu(Menu::Profile profile);
    std::shared_ptr<State> state() { return m_state; }

private:
    std::shared_ptr<Actions> m_actions;
    std::shared_ptr<State> m_state;
};

} // namespace datetime
} // namespace indicator
} // namespace unity

#endif // INDICATOR_DATETIME_MENU_H
