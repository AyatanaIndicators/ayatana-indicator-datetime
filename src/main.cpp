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

#include <datetime/actions-live.h>
#include <datetime/clock.h>
#include <datetime/formatter.h>
#include <datetime/locations-settings.h>
#include <datetime/menu.h>
#include <datetime/planner-eds.h>
#include <datetime/service.h>
#include <datetime/settings-live.h>
#include <datetime/state.h>
#include <datetime/timezones-live.h>

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <libnotify/notify.h> 

#include <iostream>

#include <locale.h>
#include <stdlib.h> /* exit() */

using namespace unity::indicator::datetime;

int
main(int /*argc*/, char** /*argv*/)
{
    // Work around a deadlock in glib's type initialization.
    // It can be removed when https://bugzilla.gnome.org/show_bug.cgi?id=674885 is fixed.
    g_type_ensure(G_TYPE_DBUS_CONNECTION);

    // boilerplate i18n
    setlocale(LC_ALL, "");
    bindtextdomain(GETTEXT_PACKAGE, GNOMELOCALEDIR);
    textdomain(GETTEXT_PACKAGE);

    // init libnotify
    if(!notify_init("indicator-datetime-service"))
        g_critical("libnotify initialization failed");

    // build the state
    std::shared_ptr<Settings> settings(new LiveSettings);
    std::shared_ptr<Timezones> timezones(new LiveTimezones(settings, TIMEZONE_FILE));
    std::shared_ptr<Clock> clock(new LiveClock(timezones));
    std::shared_ptr<Planner> planner(new PlannerEds);
    planner->time = clock->localtime();
    std::shared_ptr<Locations> locations(new SettingsLocations(settings, timezones));
    std::shared_ptr<State> state(new State);
    state->settings = settings;
    state->timezones = timezones;
    state->clock = clock;
    state->locations = locations;
    state->planner = planner;
    state->calendar_day = clock->localtime();

    // build the menu factory
    std::shared_ptr<Actions> actions(new LiveActions(state));
    MenuFactory factory(actions, state);

    // create the menus
    std::vector<std::shared_ptr<Menu>> menus;
    menus.push_back(factory.buildMenu(Menu::Desktop));

    // export them
    auto loop = g_main_loop_new(nullptr, false);
    Service service;
    service.name_lost.connect([loop](){
      g_message("exiting: service couldn't acquire or lost ownership of busname");
      g_main_loop_quit(loop);
    });
    //service.publish(actions, menus);
    g_main_loop_run(loop);
    g_main_loop_unref(loop);
    return 0;
}
