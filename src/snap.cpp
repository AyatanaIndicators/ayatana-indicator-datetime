/*
 * Copyright 2014 Canonical Ltd.
 * Copyright 2021 Robert Tari
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
 *   Robert Tari <robert@tari.in>
 */

#ifdef HAS_LOMIRI_SCHEMAS
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

namespace ain = ayatana::indicator::notifications;

namespace ayatana {
namespace indicator {
namespace datetime {

/***
****
***/

class Snap::Impl
{
public:

    Impl(const std::shared_ptr<ayatana::indicator::notifications::Engine>& engine,
         const std::shared_ptr<ayatana::indicator::notifications::SoundBuilder>& sound_builder,
         const std::shared_ptr<const Settings>& settings,
         GDBusConnection* system_bus):
      m_engine(engine),
      m_sound_builder(sound_builder),
      m_settings(settings),
      m_cancellable(g_cancellable_new()),
      m_system_bus{G_DBUS_CONNECTION(g_object_ref(system_bus))}
    {
        auto object_path = g_strdup_printf("/org/freedesktop/Accounts/User%lu", (gulong)getuid());


        accounts_service_sound_proxy_new(m_system_bus,
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
        g_clear_object(&m_system_bus);

        for (const auto& key : m_notifications)
            m_engine->close (key);
    }

    void operator()(const Appointment& appointment,
                    const Alarm& alarm,
                    response_func on_response)
    {
        // If calendar notifications are disabled, don't show them
        if (!appointment.is_alarm() && !calendar_notifications_are_enabled()) {
            g_debug("Skipping disabled calendar event '%s' notification", appointment.summary.c_str());
            return;
        }

        /* Alarms and calendar events are treated differently.
           Alarms should require manual intervention to dismiss.
           Calendar events are less urgent and shouldn't require manual
           intervention and shouldn't loop the sound. */
        const bool interactive = appointment.is_alarm() && m_engine->supports_actions();

        // force the system to stay awake
        std::shared_ptr<ain::Awake> awake;
        if (appointment.is_alarm() || calendar_bubbles_enabled() || calendar_list_enabled()) {
            awake = std::make_shared<ain::Awake>(m_system_bus, m_engine->app_name());
        }

        // calendar events are muted in silent mode; alarm clocks never are
        std::shared_ptr<ain::Sound> sound;
        if (appointment.is_alarm() || (calendar_sounds_enabled() && !silent_mode())) {
            // create the sound.
            const auto role = appointment.is_alarm() ? "alarm" : "alert";
            const auto uri = get_alarm_uri(appointment, alarm, m_settings);
            const auto volume = m_settings->alarm_volume.get();
            const bool loop = interactive;
            sound = m_sound_builder->create(role, uri, volume, loop);
        }

        // create the haptic feedback...
        std::shared_ptr<ain::Haptic> haptic;
        if (should_vibrate() && (appointment.is_alarm() || calendar_vibrations_enabled())) {
            // when in silent mode should only vibrate if user defined so
            if (!silent_mode() || vibrate_in_silent_mode_enabled()) {
                const auto haptic_mode = m_settings->alarm_haptic.get();
                if (haptic_mode == "pulse")
                    haptic = std::make_shared<ain::Haptic>(appointment.is_alarm());
            }
        }

        // show a notification...
        const auto minutes = std::chrono::minutes(m_settings->alarm_duration.get());
        ain::Builder b;
        b.set_body (appointment.summary);
        b.set_icon_name (appointment.is_alarm() ? "alarm-clock" : "calendar-app");
        b.add_hint (ain::Builder::HINT_NONSHAPED_ICON);
        b.set_start_time (appointment.begin.to_unix());

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

        const char * titlefmt;
        if (appointment.is_alarm()) {
            titlefmt = _("Alarm %s");
        } else {
            titlefmt = _("Event %s");
        }
        auto title = g_strdup_printf(titlefmt, timestr.c_str());
        b.set_title (title);
        g_free (title);
        b.set_timeout (std::chrono::duration_cast<std::chrono::seconds>(minutes));
        if (interactive) {
            b.add_hint (ain::Builder::HINT_SNAP);
            b.add_hint (ain::Builder::HINT_AFFIRMATIVE_HINT);
            b.add_action (ACTION_NONE, _("OK"));
            b.add_action (ACTION_SNOOZE, _("Snooze"));
        } else {
            b.add_hint (ain::Builder::HINT_INTERACTIVE);
            b.add_action (ACTION_SHOW_APP, _("OK"));
        }

        // add 'sound', 'haptic', and 'awake' objects to the capture so
        // they stay alive until the closed callback is called; i.e.,
        // for the lifespan of the notficiation
        b.set_closed_callback([appointment, alarm, on_response, sound, awake, haptic]
                              (const std::string& action){
            Snap::Response response;
            if ((action == ACTION_SNOOZE) || (appointment.is_alarm() && action.empty()))
                response = Snap::Response::Snooze;
            else if (action == ACTION_SHOW_APP)
                response = Snap::Response::ShowApp;
            else
                response = Snap::Response::None;

            on_response(appointment, alarm, response);
        });

        //TODO: we need to extend it to support alarms appointments
        if (!appointment.is_alarm()) {
            b.set_timeout_callback([appointment, alarm, on_response](){
                on_response(appointment, alarm, Snap::Response::ShowApp);
            });
        }

        b.set_show_notification_bubble(appointment.is_alarm() || calendar_bubbles_enabled());
        b.set_post_to_messaging_menu(appointment.is_alarm() || calendar_list_enabled());

        const auto key = m_engine->show(b);
        if (key)
            m_notifications.insert (key);
    }

private:

    bool calendar_notifications_are_enabled() const
    {
        return m_settings->cal_notification_enabled.get();
    }

    bool calendar_sounds_enabled() const
    {
        return m_settings->cal_notification_sounds.get();
    }

    bool calendar_vibrations_enabled() const
    {
        return m_settings->cal_notification_vibrations.get();
    }

    bool calendar_bubbles_enabled() const
    {
        return m_settings->cal_notification_bubbles.get();
    }

    bool calendar_list_enabled() const
    {
        return m_settings->cal_notification_list.get();
    }

    bool vibrate_in_silent_mode_enabled() const
    {
        return m_settings->vibrate_silent_mode.get();
    }

    static void on_sound_proxy_ready(GObject* /*source_object*/, GAsyncResult* res, gpointer gself)
    {
        GError * error;

        error = nullptr;
        auto proxy = accounts_service_sound_proxy_new_finish (res, &error);
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

    std::string get_alarm_uri(const Appointment& appointment,
                              const Alarm& alarm,
                              const std::shared_ptr<const Settings>& settings) const
    {
        const auto is_alarm = appointment.is_alarm();
        const std::string candidates[] = {
            alarm.audio_url,
            is_alarm ? settings->alarm_sound.get() : settings->calendar_sound.get(),
            is_alarm ? ALARM_DEFAULT_SOUND : CALENDAR_DEFAULT_SOUND
        };

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

    const std::shared_ptr<ayatana::indicator::notifications::Engine> m_engine;
    const std::shared_ptr<ayatana::indicator::notifications::SoundBuilder> m_sound_builder;
    const std::shared_ptr<const Settings> m_settings;
    std::set<int> m_notifications;
    GCancellable * m_cancellable {nullptr};
    AccountsServiceSound * m_accounts_service_sound_proxy {nullptr};
    GDBusConnection * m_system_bus {nullptr};

    static constexpr char const * ACTION_NONE {"none"};
    static constexpr char const * ACTION_SNOOZE {"snooze"};
    static constexpr char const * ACTION_SHOW_APP {"show-app"};
};

/***
****
***/

Snap::Snap(const std::shared_ptr<ayatana::indicator::notifications::Engine>& engine,
           const std::shared_ptr<ayatana::indicator::notifications::SoundBuilder>& sound_builder,
           const std::shared_ptr<const Settings>& settings,
           GDBusConnection* system_bus):
  impl(new Impl(engine, sound_builder, settings, system_bus))
{
}

Snap::~Snap()
{
}

void
Snap::operator()(const Appointment& appointment,
                 const Alarm& alarm,
                 response_func on_response)
{
  (*impl)(appointment, alarm, on_response);
}

/***
****
***/

} // namespace datetime
} // namespace indicator
} // namespace ayatana

#endif
