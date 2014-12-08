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
#include <datetime/alarm-queue-simple.h>
#include <datetime/clock.h>
#include <datetime/engine-mock.h>
#include <datetime/engine-eds.h>
#include <datetime/exporter.h>
#include <datetime/locations-settings.h>
#include <datetime/menu.h>
#include <datetime/planner-aggregate.h>
#include <datetime/planner-snooze.h>
#include <datetime/planner-range.h>
#include <datetime/settings-live.h>
#include <datetime/snap.h>
#include <datetime/state.h>
#include <datetime/timezone-file.h>
#include <datetime/timezones-live.h>
#include <datetime/wakeup-timer-powerd.h>
#include <notifications/notifications.h>

#include <glib/gi18n.h> // bindtextdomain()
#include <gio/gio.h>

#include <locale.h>
#include <cstdlib> // exit()

namespace uin = unity::indicator::notifications;

using namespace unity::indicator::datetime;

namespace
{
    std::shared_ptr<Engine> create_engine()
    {
        std::shared_ptr<Engine> engine;

        // we don't show appointments in the greeter,
        // so no need to connect to EDS there...
        if (!g_strcmp0("lightdm", g_get_user_name()))
            engine.reset(new MockEngine);
        else
            engine.reset(new EdsEngine);

        return engine;
    }

    std::shared_ptr<State> create_state(const std::shared_ptr<Engine>& engine,
                                        const std::shared_ptr<Timezone>& timezone_)
    {
        // create the live objects
        auto live_settings = std::make_shared<LiveSettings>();
        auto live_timezones = std::make_shared<LiveTimezones>(live_settings, TIMEZONE_FILE);
        auto live_clock = std::make_shared<LiveClock>(timezone_);

        // create a full-month planner currently pointing to the current month
        const auto now = live_clock->localtime();
        auto range_planner = std::make_shared<SimpleRangePlanner>(engine, timezone_);
        auto calendar_month = std::make_shared<MonthPlanner>(range_planner, now);

        // create an upcoming-events planner currently pointing to the current date
        range_planner = std::make_shared<SimpleRangePlanner>(engine, timezone_);
        auto calendar_upcoming = std::make_shared<UpcomingPlanner>(range_planner, now);

        // create the state
        auto state = std::make_shared<State>();
        state->settings = live_settings;
        state->clock = live_clock;
        state->locations = std::make_shared<SettingsLocations>(live_settings, live_timezones);
        state->calendar_month = calendar_month;
        state->calendar_upcoming = calendar_upcoming;
        return state;
    }

    std::shared_ptr<AlarmQueue> create_simple_alarm_queue(const std::shared_ptr<Clock>& clock,
                                                          const std::shared_ptr<Planner>& snooze_planner,
                                                          const std::shared_ptr<Engine>& engine,
                                                          const std::shared_ptr<Timezone>& tz)
    {
        // create an upcoming-events planner that =always= tracks the clock's date
        auto range_planner = std::make_shared<SimpleRangePlanner>(engine, tz);
        auto upcoming_planner = std::make_shared<UpcomingPlanner>(range_planner, clock->localtime());
        clock->date_changed.connect([clock,upcoming_planner](){
            const auto now = clock->localtime();
            g_debug("refretching appointments due to date change: %s", now.format("%F %T").c_str());
            upcoming_planner->date().set(now);
        });

        // create an aggregate planner that folds together the above
        // upcoming-events planner and locally-generated snooze events
        std::shared_ptr<AggregatePlanner> planner = std::make_shared<AggregatePlanner>();
        planner->add(upcoming_planner);
        planner->add(snooze_planner);

        auto wakeup_timer = std::make_shared<PowerdWakeupTimer>(clock);
        return std::make_shared<SimpleAlarmQueue>(clock, planner, wakeup_timer);
    }
}

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

    auto engine = create_engine();
    auto timezone_ = std::make_shared<FileTimezone>(TIMEZONE_FILE);
    auto state = create_state(engine, timezone_);
    auto actions = std::make_shared<LiveActions>(state);
    MenuFactory factory(actions, state);

    // set up the snap decisions
    auto snooze_planner = std::make_shared<SnoozePlanner>(state->settings, state->clock);
    auto notification_engine = std::make_shared<uin::Engine>("indicator-datetime-service");
    std::unique_ptr<Snap> snap (new Snap(notification_engine, state->settings));
    auto alarm_queue = create_simple_alarm_queue(state->clock, snooze_planner, engine, timezone_);
    auto on_snooze = [snooze_planner](const Appointment& a) {snooze_planner->add(a);};
    auto on_ok = [](const Appointment&){};
    auto on_alarm_reached = [&engine, &snap, &on_snooze, &on_ok](const Appointment& a) {
        (*snap)(a, on_snooze, on_ok);
        engine->disable_ubuntu_alarm(a);
    };
    alarm_queue->alarm_reached().connect(on_alarm_reached);

    // create the menus
    std::vector<std::shared_ptr<Menu>> menus;
    for(int i=0, n=Menu::NUM_PROFILES; i<n; i++)
        menus.push_back(factory.buildMenu(Menu::Profile(i)));

    // export them & run until we lose the busname
    auto loop = g_main_loop_new(nullptr, false);
    Exporter exporter(state->settings);
    exporter.name_lost().connect([loop](){
        g_message("%s exiting; failed/lost bus ownership", GETTEXT_PACKAGE);
        g_main_loop_quit(loop);
    });
    exporter.publish(actions, menus);
    g_main_loop_run(loop);

    g_main_loop_unref(loop);
    return 0;
}
