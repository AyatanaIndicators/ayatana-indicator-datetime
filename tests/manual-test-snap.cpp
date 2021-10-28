
/*
 * Copyright 2013 Canonical Ltd.
 * Copyright 2021 Robert Tari
 *
 * Authors:
 *   Charles Kerr <charles.kerr@canonical.com>
 *   Robert Tari <robert@tari.in>
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

#include <datetime/appointment.h>
#include <datetime/settings-live.h>
#include <datetime/snap.h>
#include <datetime/timezones-live.h>
#include <notifications/notifications.h>

#include <glib.h>

using namespace ayatana::indicator::datetime;

namespace ain = ayatana::indicator::notifications;

/***
****
***/

namespace
{
    gboolean quit_idle (gpointer gloop)
    {
        g_main_loop_quit(static_cast<GMainLoop*>(gloop));
        return G_SOURCE_REMOVE;
    };

    int volume = 50;

    GOptionEntry entries[] =
    {
        { "volume", 'v', 0, G_OPTION_ARG_INT, &volume, "Volume level [1..100]", "volume" },
        { nullptr }
    };
}

int main(int argc, const char* argv[])
{
    GError* error = nullptr;
    GOptionContext* context = g_option_context_new(nullptr);
    g_option_context_add_main_entries(context, entries, nullptr);
    if (!g_option_context_parse(context, &argc, (gchar***)&argv, &error))
    {
        g_print("option parsing failed: %s\n", error->message);
        exit(1);
    }
    g_option_context_free(context);
    volume = CLAMP(volume, 1, 100);

    Appointment a;
    a.color = "green";
    a.summary = "Alarm";
    a.uid = "D4B57D50247291478ED31DED17FF0A9838DED402";
    a.type = Appointment::ALARM;
    a.begin = DateTime::Local(2014, 12, 25, 0, 0, 0);
    a.end = a.begin.end_of_day();
    a.alarms.push_back(Alarm{"Alarm Text", "", a.begin});

    auto loop = g_main_loop_new(nullptr, false);
    auto on_response = [loop](const Appointment& appt, const Alarm&, const Snap::Response& response){
        const char* str {""};
        switch(response) {
            case Snap::Response::ShowApp: str = "show-app"; break;
            case Snap::Response::Snooze: str = "snooze"; break;
            case Snap::Response::None: str = "no-action"; break;
        };
        g_message("You clicked '%s' for appt url '%s'", str, appt.summary.c_str());
        g_idle_add(quit_idle, loop);
    };

    // only use local, temporary settings
    g_assert(g_setenv("GSETTINGS_SCHEMA_DIR", SCHEMA_DIR, true));
    g_assert(g_setenv("GSETTINGS_BACKEND", "memory", true));
    g_debug("SCHEMA_DIR is %s", SCHEMA_DIR);

    auto settings = std::make_shared<LiveSettings>();
    settings->alarm_volume.set(volume);

    auto notification_engine = std::make_shared<ain::Engine>("ayatana-indicator-datetime-service");
    auto sound_builder = std::make_shared<ain::DefaultSoundBuilder>();
    auto system_bus = g_bus_get_sync(G_BUS_TYPE_SYSTEM, nullptr, nullptr);
    Snap snap (notification_engine, sound_builder, settings, system_bus);
    snap(a, a.alarms.front(), on_response);
    g_main_loop_run(loop);

    g_main_loop_unref(loop);
    g_clear_object(&system_bus);
    return 0;
}
