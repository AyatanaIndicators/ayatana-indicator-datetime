/*
 * Copyright 2014 Canonical Ltd.
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

#include <datetime/snap.h>

#include <notifications/awake.h>
#include <notifications/sound.h>

#include <gst/gst.h>

#include <glib/gi18n.h>

#include <chrono>
#include <string>

namespace uin = unity::indicator::notifications;

namespace unity {
namespace indicator {
namespace datetime {

/***
****
***/

static std::string get_alarm_uri(const Appointment& appointment,
                                 const std::shared_ptr<const Settings>& settings)
{
    const char* FALLBACK {"/usr/share/sounds/ubuntu/ringtones/Suru arpeggio.ogg"};

    const std::string candidates[] = { appointment.audio_url,
                                       settings->alarm_sound.get(),
                                       FALLBACK };

    std::string uri;

    for(const auto& candidate : candidates)
    {
        if (gst_uri_is_valid (candidate.c_str()))
        {
            uri = candidate;
            break;
        }
        else if (g_file_test(candidate.c_str(), G_FILE_TEST_EXISTS))
        {
            gchar* tmp = gst_filename_to_uri(candidate.c_str(), nullptr);
            if (tmp != nullptr)
            {
                uri = tmp;
                g_free (tmp);
                break;
            }
        }
    }

    return uri;
}

/***
****
***/

Snap::Snap(const std::shared_ptr<uin::Engine>& engine,
           const std::shared_ptr<const Settings>& settings):
    m_engine(engine),
    m_settings(settings)
{
}

Snap::~Snap()
{
    for (const auto& key : m_notifications)
        m_engine->close (key);
}

void Snap::operator()(const Appointment& appointment,
                      appointment_func show,
                      appointment_func dismiss)
{
    if (!appointment.has_alarms)
    {
        dismiss(appointment);
        return;
    }

    // force the system to stay awake
    auto awake = std::make_shared<uin::Awake>(m_engine->app_name());

    // create the sound...,
    const auto uri = get_alarm_uri(appointment, m_settings);
    const auto volume = m_settings->alarm_volume.get();
    const bool loop = m_engine->supports_actions();
    auto sound = std::make_shared<uin::Sound>(uri, volume, loop);

    // show a notification...
    const auto minutes = m_settings->alarm_duration.get();
    const bool interactive = m_engine->supports_actions();
    uin::Builder notification_builder;
    notification_builder.set_body (appointment.summary);
    notification_builder.set_icon_name ("alarm-clock");
    notification_builder.add_hint (uin::Builder::HINT_SNAP);
    notification_builder.add_hint (uin::Builder::HINT_TINT);
    const auto timestr = appointment.begin.format (_("%a, %X"));
    auto title = g_strdup_printf (_("Alarm %s"), timestr.c_str());
    notification_builder.set_title (title);
    g_free (title);
    notification_builder.set_timeout (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::minutes(minutes)));
    if (interactive) {
        notification_builder.add_action ("show", _("Show"));
        notification_builder.add_action ("dismiss", _("Dismiss"));
    }

    // add the 'sound' and 'awake' objects to the capture so that
    // they stay alive until the closed callback is called; i.e.,
    // for the lifespan of the notficiation
    notification_builder.set_closed_callback([appointment,
                                              show,
                                              dismiss,
                                              sound,
                                              awake](const std::string& action){
        if (action == "show")
            show(appointment);
        else
            dismiss(appointment);
    });

    const auto key = m_engine->show (notification_builder);
    if (key)
        m_notifications.insert (key);
    else
        show(appointment);
}

/***
****
***/

} // namespace datetime
} // namespace indicator
} // namespace unity
