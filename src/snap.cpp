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

#include "dbus-accounts-sound.h"

#include <datetime/snap.h>
#include <datetime/utils.h> // is_locale_12h()

#include <notifications/awake.h>
#include <notifications/haptic.h>
#include <notifications/sound.h>

#include <gst/gst.h>

#include <glib/gi18n.h>

#include <chrono>
#include <set>
#include <string>

#include <unistd.h> // getuid()
#include <sys/types.h> // getuid()

namespace uin = unity::indicator::notifications;

namespace ayatana {
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
      m_settings(settings),
      m_cancellable(g_cancellable_new())
    {
        auto object_path = g_strdup_printf("/org/freedesktop/Accounts/User%lu", (gulong)getuid());
        accounts_service_sound_proxy_new_for_bus(G_BUS_TYPE_SYSTEM,
                                                 G_DBUS_PROXY_FLAGS_GET_INVALIDATED_PROPERTIES,
                                                 "org.freedesktop.Accounts",
                                                 object_path,
                                                 m_cancellable,
                                                 on_sound_proxy_ready,
                                                 this);
        g_free(object_path);
    }

    ~Impl()
    {
        g_cancellable_cancel(m_cancellable);
        g_clear_object(&m_cancellable);
        g_clear_object(&m_accounts_service_sound_proxy);

        for (const auto& key : m_notifications)
            m_engine->close (key);
    }

    void operator()(const Appointment& appointment,
                    const Alarm& alarm,
                    appointment_func snooze,
                    appointment_func ok)
    {
        /* Alarms and calendar events are treated differently.
           Alarms should require manual intervention to dismiss.
           Calendar events are less urgent and shouldn't require manual
           intervention and shouldn't loop the sound. */
        const bool interactive = appointment.is_ubuntu_alarm() && m_engine->supports_actions();

        // force the system to stay awake
        auto awake = std::make_shared<uin::Awake>(m_engine->app_name());

        // calendar events are muted in silent mode; alarm clocks never are
        std::shared_ptr<uin::Sound> sound;
        if (appointment.is_ubuntu_alarm() || !silent_mode()) {
            // create the sound.
            const auto role = appointment.is_ubuntu_alarm() ? "alarm" : "alert";
            const auto uri = get_alarm_uri(alarm, m_settings);
            const auto volume = m_settings->alarm_volume.get();
            const bool loop = interactive;
            sound = std::make_shared<uin::Sound>(role, uri, volume, loop);
        }

        // create the haptic feedback...
        std::shared_ptr<uin::Haptic> haptic;
        if (should_vibrate()) {
            const auto haptic_mode = m_settings->alarm_haptic.get();
            if (haptic_mode == "pulse")
                haptic = std::make_shared<uin::Haptic>(uin::Haptic::MODE_PULSE);
        }

        // show a notification...
        const auto minutes = std::chrono::minutes(m_settings->alarm_duration.get());
        uin::Builder b;
        b.set_body (appointment.summary);
        b.set_icon_name ("alarm-clock");
        b.add_hint (uin::Builder::HINT_NONSHAPED_ICON);

        const char * timefmt;
        if (is_locale_12h()) {
            /** strftime(3) format for abbreviated weekday,
                hours, minutes in a 12h locale; e.g. Wed, 2:00 PM */
            timefmt = _("%a, %l:%M %p");
        } else {
            /** A strftime(3) format for abbreviated weekday,
                hours, minutes in a 24h locale; e.g. Wed, 14:00 */
            timefmt = _("%a, %H:%M");
        }
        const auto timestr = appointment.begin.format(timefmt);
        auto title = g_strdup_printf(_("Alarm %s"), timestr.c_str());
        b.set_title (title);
        g_free (title);
        b.set_timeout (std::chrono::duration_cast<std::chrono::seconds>(minutes));
        if (interactive) {
            b.add_hint (uin::Builder::HINT_SNAP);
            b.add_hint (uin::Builder::HINT_AFFIRMATIVE_HINT);
            b.add_action ("ok", _("OK"));
            b.add_action ("snooze", _("Snooze"));
        }

        // add 'sound', 'haptic', and 'awake' objects to the capture so
        // they stay alive until the closed callback is called; i.e.,
        // for the lifespan of the notficiation
        b.set_closed_callback([appointment, alarm, snooze, ok, sound, awake, haptic]
                              (const std::string& action){
            if (action == "snooze")
                snooze(appointment, alarm);
            else
                ok(appointment, alarm);
        });

        const auto key = m_engine->show(b);
        if (key)
            m_notifications.insert (key);
    }

private:

    static void on_sound_proxy_ready(GObject* /*source_object*/, GAsyncResult* res, gpointer gself)
    {
        GError * error;

        error = nullptr;
        auto proxy = accounts_service_sound_proxy_new_for_bus_finish (res, &error);
        if (error != nullptr)
        {
            if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                g_warning("%s Couldn't find accounts service sound proxy: %s", G_STRLOC, error->message);

            g_clear_error(&error);
        }
        else
        {
            static_cast<Impl*>(gself)->m_accounts_service_sound_proxy = proxy;
        }
    }

    bool silent_mode() const
    {
        return (m_accounts_service_sound_proxy != nullptr)
            && (accounts_service_sound_get_silent_mode(m_accounts_service_sound_proxy));
    }

    bool should_vibrate() const
    {
        return (m_accounts_service_sound_proxy != nullptr)
            && (accounts_service_sound_get_other_vibrate(m_accounts_service_sound_proxy));
    }

    std::string get_alarm_uri(const Alarm& alarm,
                              const std::shared_ptr<const Settings>& settings) const
    {
        const char* FALLBACK {"/usr/share/sounds/ubuntu/ringtones/Alarm clock.ogg"};

        const std::string candidates[] = { alarm.audio_url,
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
    GCancellable * m_cancellable {nullptr};
    AccountsServiceSound * m_accounts_service_sound_proxy {nullptr};
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
                 const Alarm& alarm,
                 appointment_func show,
                 appointment_func ok)
{
  (*impl)(appointment, alarm, show, ok);
}

/***
****
***/

} // namespace datetime
} // namespace indicator
} // namespace ayatana
