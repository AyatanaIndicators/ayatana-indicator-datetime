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
#include <notifications/haptic.h>
#include <notifications/sound.h>

#include <gst/gst.h>

#include <glib/gi18n.h>

#include <chrono>
#include <set>
#include <string>

namespace uin = unity::indicator::notifications;

namespace unity {
namespace indicator {
namespace datetime {

/***
****
***/

class Snap::Impl
{
public:

    Impl(const std::shared_ptr<unity::indicator::notifications::Engine>& engine,
         const std::shared_ptr<const Settings>& settings):
      m_engine(engine),
      m_settings(settings)
    {
    }

    ~Impl()
    {
        for (const auto& key : m_notifications)
            m_engine->close (key);
    }

    void operator()(const Appointment& appointment,
                    appointment_func snooze,
                    appointment_func ok)
    {
        if (!appointment.has_alarms)
        {
            ok(appointment);
            return;
        }

        // force the system to stay awake
        auto awake = std::make_shared<uin::Awake>(m_engine->app_name());

        // create the sound...
        const auto uri = get_alarm_uri(appointment, m_settings);
        const auto volume = m_settings->alarm_volume.get();
        const bool loop = m_engine->supports_actions();
        auto sound = std::make_shared<uin::Sound>(uri, volume, loop);

        // create the haptic feedback...
        const auto haptic_mode = m_settings->alarm_haptic.get();
        std::shared_ptr<uin::Haptic> haptic;
        if (haptic_mode == "pulse")
            haptic = std::make_shared<uin::Haptic>(uin::Haptic::MODE_PULSE);

        // show a notification...
        const auto minutes = std::chrono::minutes(m_settings->alarm_duration.get());
        const bool interactive = m_engine->supports_actions();
        uin::Builder b;
        b.set_body (appointment.summary);
        b.set_icon_name ("alarm-clock");
        b.add_hint (uin::Builder::HINT_SNAP);
        b.add_hint (uin::Builder::HINT_TINT);
        b.add_hint (uin::Builder::HINT_NONSHAPEDICON);
        const auto timestr = appointment.begin.format (_("%a, %X"));
        auto title = g_strdup_printf (_("Alarm %s"), timestr.c_str());
        b.set_title (title);
        g_free (title);
        b.set_timeout (std::chrono::duration_cast<std::chrono::seconds>(minutes));
        if (interactive) {
            b.add_action ("snooze", _("Snooze"));
            b.add_action ("ok", _("OK"));
        }

        // add 'sound', 'haptic', and 'awake' objects to the capture so
        // they stay alive until the closed callback is called; i.e.,
        // for the lifespan of the notficiation
        b.set_closed_callback([appointment, snooze, ok, sound, awake, haptic]
                              (const std::string& action){
            if (action == "snooze")
                snooze(appointment);
            else
                ok(appointment);
        });

        const auto key = m_engine->show(b);
        if (key)
            m_notifications.insert (key);
    }

private:

    std::string get_alarm_uri(const Appointment& appointment,
                              const std::shared_ptr<const Settings>& settings) const
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

    const std::shared_ptr<unity::indicator::notifications::Engine> m_engine;
    const std::shared_ptr<const Settings> m_settings;
    std::set<int> m_notifications;
};

/***
****
***/

Snap::Snap(const std::shared_ptr<unity::indicator::notifications::Engine>& engine,
           const std::shared_ptr<const Settings>& settings):
  impl(new Impl(engine, settings))
{
}

Snap::~Snap()
{
}

void
Snap::operator()(const Appointment& appointment,
                 appointment_func show,
                 appointment_func ok)
{
  (*impl)(appointment, show, ok);
}

/***
****
***/

} // namespace datetime
} // namespace indicator
} // namespace unity
