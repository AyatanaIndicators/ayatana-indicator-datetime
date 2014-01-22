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
#include <datetime/exporter.h>
#include <datetime/menu.h>
#include <datetime/state-live.h>

#include <glib/gi18n.h> // bindtextdomain()
#include <gio/gio.h>
#include <libnotify/notify.h> 

#include <locale.h>
#include <stdlib.h> // exit()

using namespace unity::indicator::datetime;

int
main(int /*argc*/, char** /*argv*/)
{
    // Work around a deadlock in glib's type initialization.
    // It can be removed when https://bugzilla.gnome.org/show_bug.cgi?id=674885 is fixed.
    g_type_ensure(G_TYPE_DBUS_CONNECTION);

    // init i18n
    setlocale(LC_ALL, "");
    bindtextdomain(GETTEXT_PACKAGE, GNOMELOCALEDIR);
    textdomain(GETTEXT_PACKAGE);

    // init libnotify
    if(!notify_init("indicator-datetime-service"))
        g_critical("libnotify initialization failed");

    // build the state and menu factory
    std::shared_ptr<State> state(new LiveState);
    std::shared_ptr<Actions> actions(new LiveActions(state));
    MenuFactory factory(actions, state);

    // create the menus
    std::vector<std::shared_ptr<Menu>> menus;
    menus.push_back(factory.buildMenu(Menu::Desktop));

    // export them
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
