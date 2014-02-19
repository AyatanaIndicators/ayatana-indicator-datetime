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
#include <datetime/clock-watcher.h>
#include <datetime/exporter.h>
#include <datetime/locations-settings.h>
#include <datetime/menu.h>
#include <datetime/planner-eds.h>
#include <datetime/settings-live.h>
#include <datetime/snap.h>
#include <datetime/state.h>
#include <datetime/timezones-live.h>

#include <glib/gi18n.h> // bindtextdomain()
#include <gio/gio.h>

#include <url-dispatcher.h>

#include <locale.h>
#include <cstdlib> // exit()

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

    // build the state, actions, and menufactory
    std::shared_ptr<State> state(new State);
    std::shared_ptr<Settings> live_settings(new LiveSettings);
    std::shared_ptr<Timezones> live_timezones(new LiveTimezones(live_settings, TIMEZONE_FILE));
    std::shared_ptr<Clock> live_clock(new LiveClock(live_timezones));
    state->settings = live_settings;
    state->clock = live_clock;
    state->locations.reset(new SettingsLocations(live_settings, live_timezones));
    state->planner.reset(new PlannerEds(live_clock));
    state->planner->time = live_clock->localtime();
    std::shared_ptr<Actions> actions(new LiveActions(state));
    MenuFactory factory(actions, state);

    // snap decisions
    ClockWatcherImpl clock_watcher(state);
    Snap snap;
    clock_watcher.alarm_reached().connect([&snap](const Appointment& appt){
        auto snap_show = [](const Appointment& a){
            const char* url;
            if(!a.url.empty())
                url = a.url.c_str();
            else // alarm doesn't have a URl associated with it; use a fallback
                url = "appid://com.ubuntu.clock/clock/current-user-version";
            url_dispatch_send(url, nullptr, nullptr);
        };
        auto snap_dismiss = [](const Appointment&){};
        snap(appt, snap_show, snap_dismiss);
    });

    // create the menus
    std::vector<std::shared_ptr<Menu>> menus;
    for(int i=0, n=Menu::NUM_PROFILES; i<n; i++)
        menus.push_back(factory.buildMenu(Menu::Profile(i)));

    // export them & run until we lose the busname
    auto loop = g_main_loop_new(nullptr, false);
    Exporter exporter;
    exporter.name_lost.connect([loop](){
        g_message("%s exiting; failed/lost bus ownership", GETTEXT_PACKAGE);
        g_main_loop_quit(loop);
    });
    exporter.publish(actions, menus);
    g_main_loop_run(loop);
    g_main_loop_unref(loop);
    return 0;
}
