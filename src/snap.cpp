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

#include <datetime/appointment.h>
#include <datetime/formatter.h>
#include <datetime/snap.h>

#include <core/signal.h>

#include <canberra.h>
#include <libnotify/notify.h>

#include <glib/gi18n.h>
#include <glib.h>

#include <set>
#include <string>

namespace unity {
namespace indicator {
namespace datetime {

/***
****
***/

namespace
{

/**
 * Plays a sound, possibly looping.
 */
class Sound
{
    typedef Sound Self;

public:

    Sound(const std::shared_ptr<Clock>& clock,
          const std::string& filename,
          int volume,
          int duration_minutes,
          bool loop):
        m_clock(clock),
        m_filename(filename),
        m_volume(volume),
        m_loop(loop),
        m_canberra_id(get_next_canberra_id()),
        m_loop_end_time(clock->localtime().add_full(0, 0, 0, 0, duration_minutes, 0.0))
    {
        if (m_loop)
        {
            g_debug("Looping '%s' until cutoff time %s",
                    m_filename.c_str(),
                    m_loop_end_time.format("%F %T").c_str());
        }
        else
        {
            g_debug("Playing '%s' once", m_filename.c_str());
        }

        const auto rv = ca_context_create(&m_context);
        if (rv == CA_SUCCESS)
        {
            play();
        }
        else
        {
            g_warning("Failed to create canberra context: %s", ca_strerror(rv));
            m_context = nullptr;
        }
    }

    ~Sound()
    {
        stop();

        g_clear_pointer(&m_context, ca_context_destroy);
    }

private:

    void stop()
    {
        if (m_context != nullptr)
        {
            const auto rv = ca_context_cancel(m_context, m_canberra_id);
            if (rv != CA_SUCCESS)
                g_warning("Failed to cancel alarm sound: %s", ca_strerror(rv));
        }

        if (m_loop_tag != 0)
        {
            g_source_remove(m_loop_tag);
            m_loop_tag = 0;
        }
    }

    void play()
    {
        auto context = m_context;
        g_return_if_fail(context != nullptr);

        const auto filename = m_filename.c_str();
        const float gain = get_gain_level(m_volume);

        ca_proplist* props = nullptr;
        ca_proplist_create(&props);
        ca_proplist_sets(props, CA_PROP_MEDIA_FILENAME, filename);
        ca_proplist_setf(props, CA_PROP_CANBERRA_VOLUME, "%f", gain);
        const auto rv = ca_context_play_full(context, m_canberra_id, props,
                                             on_done_playing, this);
        if (rv != CA_SUCCESS)
            g_warning("Unable to play '%s': %s", filename, ca_strerror(rv));

        g_clear_pointer(&props, ca_proplist_destroy);
    }

    static float get_gain_level(int volume)
    {
        const int clamped_volume = CLAMP(volume, 1, 100);

        /* This range isn't set in stone --
           arrived at from manual tests on Nextus 4 */
        constexpr float gain_low = -10;
        constexpr float gain_high = 10;

        constexpr float gain_range = gain_high - gain_low;
        return gain_low + (gain_range * (clamped_volume / 100.0f));
    }

    static void on_done_playing(ca_context*, uint32_t, int rv, void* gself)
    {
        // if we still need to loop, wait a second, then play it again

        if (rv == CA_SUCCESS)
        {
            auto self = static_cast<Self*>(gself);
            if ((self->m_loop_tag == 0) &&
                (self->m_loop) &&
                (self->m_clock->localtime() < self->m_loop_end_time))
            {
                self->m_loop_tag = g_timeout_add_seconds(1, play_idle, self);
            }
        }
    }

    static gboolean play_idle(gpointer gself)
    {
        auto self = static_cast<Self*>(gself);
        self->m_loop_tag = 0;
        self->play();
        return G_SOURCE_REMOVE;
    }

    /***
    ****
    ***/

    static int32_t get_next_canberra_id()
    {
        static int32_t next_canberra_id = 1;
        return next_canberra_id++;
    }

    const std::shared_ptr<Clock> m_clock;
    const std::string m_filename;
    const int m_volume;
    const bool m_loop;
    const int32_t m_canberra_id;
    const DateTime m_loop_end_time;
    ca_context* m_context = nullptr;
    guint m_loop_tag = 0;
};

class SoundBuilder
{
public:
    void set_clock(const std::shared_ptr<Clock>& c) {m_clock = c;}
    void set_filename(const std::string& s) {m_filename = s;}
    void set_volume(const int v) {m_volume = v;}
    void set_duration_minutes(int i) {m_duration_minutes=i;}
    void set_looping(bool b) {m_looping=b;}

    Sound* operator()() {
        return new Sound (m_clock,
                          m_filename,
                          m_volume,
                          m_duration_minutes,
                          m_looping);
    }

private:
    std::shared_ptr<Clock> m_clock;
    std::string m_filename;
    int m_volume = 50;
    int m_duration_minutes = 30;
    bool m_looping = true;
};

/**
 * A popup notification (with optional sound)
 * that emits a Response signal when done.
 */
class Popup
{
public:

    Popup(const Appointment& appointment, const SoundBuilder& sound_builder):
        m_appointment(appointment),
        m_interactive(get_interactive()),
        m_sound_builder(sound_builder)
    {
        // ensure notify_init() is called once
        // before we start popping up dialogs
        static bool m_nn_inited = false;
        if (G_UNLIKELY(!m_nn_inited))
        {
            if(!notify_init("indicator-datetime-service"))
                g_critical("libnotify initialization failed");

            m_nn_inited = true;
        }

        show();
    }

    ~Popup()
    {
        if (m_nn != nullptr)
        {
            notify_notification_clear_actions(m_nn);
            g_signal_handlers_disconnect_by_data(m_nn, this);
            g_clear_object(&m_nn);
        }
    }

    typedef enum
    {
        RESPONSE_SHOW,
        RESPONSE_DISMISS,
        RESPONSE_CLOSE
    }
    Response;

    core::Signal<Response>& response() { return m_response; }

private:

    void show()
    {
        const Appointment& appointment = m_appointment;

        /// strftime(3) format string for an alarm's snap decision
        const auto timestr = appointment.begin.format(_("%a, %X"));
        auto title = g_strdup_printf(_("Alarm %s"), timestr.c_str());
        const auto body = appointment.summary;
        const gchar* icon_name = "alarm-clock";

        m_nn = notify_notification_new(title, body.c_str(), icon_name);
        if (m_interactive)
        {
            notify_notification_set_hint_string(m_nn,
                                                "x-canonical-snap-decisions",
                                                "true");
            notify_notification_set_hint_string(m_nn,
                                                "x-canonical-private-button-tint",
                                                "true");
            /// alarm popup dialog's button to show the active alarm
            notify_notification_add_action(m_nn, "show", _("Show"),
                                           on_snap_show, this, nullptr);
            /// alarm popup dialog's button to shut up the alarm
            notify_notification_add_action(m_nn, "dismiss", _("Dismiss"),
                                           on_snap_dismiss, this, nullptr);
            g_signal_connect(m_nn, "closed", G_CALLBACK(on_snap_closed), this);
        }

        bool shown = true;
        GError* error = nullptr;
        notify_notification_show(m_nn, &error);
        if (error != NULL)
        {
            g_critical("Unable to show snap decision for '%s': %s",
                       body.c_str(), error->message);
            g_error_free(error);
            shown = false;
        }

        // Loop the sound *only* if we're prompting the user for a response.
        // Otherwise, just play the sound once.
        m_sound_builder.set_looping (shown && m_interactive);
        m_sound.reset (m_sound_builder());

        // if showing the notification didn't work,
        // treat it as if the user clicked the 'show' button
        if (!shown)
        {
            on_snap_show(nullptr, nullptr, this);
            on_snap_dismiss(nullptr, nullptr, this);
        }

        g_free(title);
    }

    // user clicked 'show'
    static void on_snap_show(NotifyNotification*, gchar*, gpointer gself)
    {
        auto self = static_cast<Self*>(gself);
        self->m_response_value = RESPONSE_SHOW;
        self->m_sound.reset();
    }

    // user clicked 'dismiss'
    static void on_snap_dismiss(NotifyNotification*, gchar*, gpointer gself)
    {
        auto self = static_cast<Self*>(gself);
        self->m_response_value = RESPONSE_DISMISS;
        self->m_sound.reset();
    }

    // the popup was closed
    static void on_snap_closed(NotifyNotification*, gpointer gself)
    {
        auto self = static_cast<Self*>(gself);
        self->m_sound.reset();
        self->m_response(self->m_response_value);
    }

    /***
    ****  Interactive
    ***/

    static std::set<std::string> get_server_caps()
    {
        std::set<std::string> caps_set;
        auto caps_gl = notify_get_server_caps();
        std::string caps_str;
        for(auto l=caps_gl; l!=nullptr; l=l->next)
        {
            caps_set.insert((const char*)l->data);

            caps_str += (const char*) l->data;;
            if (l->next != nullptr)
              caps_str += ", ";
        }
        g_debug("%s notify_get_server() returned [%s]", G_STRFUNC, caps_str.c_str());
        g_list_free_full(caps_gl, g_free);
        return caps_set;
    }

    static bool get_interactive()
    {
        static bool interactive;
        static bool inited = false;

        if (G_UNLIKELY(!inited))
        {
            interactive = get_server_caps().count("actions") != 0;
            inited = true;
        }

        return interactive;
    }

    /***
    ****
    ***/

    typedef Popup Self;

    const Appointment m_appointment;
    const bool m_interactive;
    SoundBuilder m_sound_builder;
    std::unique_ptr<Sound> m_sound;
    core::Signal<Response> m_response;
    Response m_response_value = RESPONSE_CLOSE;
    NotifyNotification* m_nn = nullptr;
};

/** 
***  libnotify -- snap decisions
**/

std::string get_local_filename (const std::string& str)
{
    std::string ret;

    if (!str.empty())
    {
        GFile* files[] = { g_file_new_for_path(str.c_str()),
                           g_file_new_for_uri(str.c_str()) };

        for(auto& file : files)
        {
            if (g_file_is_native(file) && g_file_query_exists(file, nullptr))
            {
                char* tmp = g_file_get_path(file);
                if (tmp != nullptr)
                {
                    ret = tmp;
                    g_free(tmp);
                    break;
                }
            }
        }

        for(auto& file : files)
            g_object_unref(file);
    }

    return ret;
}

std::string get_alarm_sound(const Appointment& appointment,
                            const std::shared_ptr<const Settings>& settings)
{
    const char* FALLBACK {"/usr/share/sounds/ubuntu/ringtones/Suru arpeggio.ogg"};

    const std::string candidates[] = { appointment.audio_url,
                                       settings->alarm_sound.get(),
                                       FALLBACK };

    std::string alarm_sound;

    for(const auto& candidate : candidates)
    {
        alarm_sound = get_local_filename(candidate);

        if (!alarm_sound.empty())
            break;
    }

    g_debug("%s: Appointment \"%s\" using alarm sound \"%s\"",
            G_STRFUNC, appointment.summary.c_str(), alarm_sound.c_str());

    return alarm_sound;
}

} // unnamed namespace

/***
****
***/

Snap::Snap(const std::shared_ptr<Clock>& clock,
           const std::shared_ptr<const Settings>& settings):
    m_clock(clock),
    m_settings(settings)
{
}

Snap::~Snap()
{
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

    // create a popup...
    SoundBuilder sound_builder;
    sound_builder.set_filename(get_alarm_sound(appointment, m_settings));
    sound_builder.set_volume(m_settings->alarm_volume.get());
    sound_builder.set_clock(m_clock);
    sound_builder.set_duration_minutes(m_settings->alarm_duration.get());
    auto popup = new Popup(appointment, sound_builder);
     
    // listen for it to finish...
    popup->response().connect([appointment,
                               show,
                               dismiss,
                               popup](Popup::Response response){
     
        // we can't delete the Popup inside its response() signal handler,
        // so push that to an idle func
        g_idle_add([](gpointer gdata){
            delete static_cast<Popup*>(gdata);
            return G_SOURCE_REMOVE;
        }, popup);

        // maybe notify the client code that the popup's done
        if (response == Popup::RESPONSE_SHOW)
            show(appointment);
        else if (response == Popup::RESPONSE_DISMISS)
            dismiss(appointment);
    });
}

/***
****
***/

} // namespace datetime
} // namespace indicator
} // namespace unity
