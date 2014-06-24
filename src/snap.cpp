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
 * Plays a sound in a loop.
 */
class LoopedSound
{
    typedef LoopedSound Self;

public:

    LoopedSound(const std::string& filename, const AlarmVolume volume):
        m_filename(filename),
        m_volume(volume),
        m_canberra_id(get_next_canberra_id())
    {
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

    ~LoopedSound()
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

        if (m_timeout_tag != 0)
        {
            g_source_remove(m_timeout_tag);
            m_timeout_tag = 0;
        }
    }

    static void on_done_playing(ca_context*, uint32_t /*id*/, int rv, void* gself)
    {
        auto self = static_cast<Self*>(gself);

        // wait a second, then play it again
        if ((rv == CA_SUCCESS) && (self->m_timeout_tag == 0))
            self->m_timeout_tag = g_timeout_add_seconds(1, play_idle, self);
    }

    static gboolean play_idle(gpointer gself)
    {
        auto self = static_cast<Self*>(gself);
        self->m_timeout_tag = 0;
        self->play();
        return G_SOURCE_REMOVE;
    }

    void play()
    {
        auto context = m_context;
        g_return_if_fail(context != nullptr);

        ca_proplist* props = nullptr;
        ca_proplist_create(&props);
        ca_proplist_sets(props, CA_PROP_MEDIA_FILENAME, m_filename.c_str());
        ca_proplist_setf(props, CA_PROP_CANBERRA_VOLUME, "%f", get_decibel_multiplier(m_volume));
        const auto rv = ca_context_play_full(context, m_canberra_id, props, on_done_playing, this);
        if (rv != CA_SUCCESS)
            g_warning("Failed to play file '%s': %s", m_filename.c_str(), ca_strerror(rv));

        g_clear_pointer(&props, ca_proplist_destroy);
    }

    static float get_decibel_multiplier(const AlarmVolume volume)
    {
        /* These values aren't set in stone -- 
           arrived at from from manual tests on Nexus 4 */
        switch (volume)
        {
            case ALARM_VOLUME_VERY_QUIET: return -8;
            case ALARM_VOLUME_QUIET:      return -4;
            case ALARM_VOLUME_VERY_LOUD:  return  8;
            case ALARM_VOLUME_LOUD:       return  4;
            default:                      return  0;
        }
    }

    /***
    ****
    ***/

    static int32_t get_next_canberra_id()
    {
        static int32_t next_canberra_id = 1;
        return next_canberra_id++;
    }

    ca_context* m_context = nullptr;
    guint m_timeout_tag = 0;
    const std::string m_filename;
    const AlarmVolume m_volume;
    const int32_t m_canberra_id;
};

/**
 * A popup notification (with optional sound)
 * that emits a Response signal when done.
 */
class Popup
{
public:

    Popup(const Appointment& appointment,
          const std::string& sound_filename,
          const AlarmVolume sound_volume):
        m_appointment(appointment),
        m_sound_filename(sound_filename),
        m_sound_volume(sound_volume),
        m_mode(get_mode())
    {
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

        const auto timestr = appointment.begin.format("%a, %X");
        auto title = g_strdup_printf(_("Alarm %s"), timestr.c_str());
        const auto body = appointment.summary;
        const gchar* icon_name = "alarm-clock";

        m_nn = notify_notification_new(title, body.c_str(), icon_name);
        if (m_mode == MODE_SNAP)
        {
            notify_notification_set_hint_string(m_nn, "x-canonical-snap-decisions", "true");
            notify_notification_set_hint_string(m_nn, "x-canonical-private-button-tint", "true");
            // text for the alarm popup dialog's button to show the active alarm
            notify_notification_add_action(m_nn, "show", _("Show"), on_snap_show, this, nullptr);
            // text for the alarm popup dialog's button to shut up the alarm
            notify_notification_add_action(m_nn, "dismiss", _("Dismiss"), on_snap_dismiss, this, nullptr);
            g_signal_connect(m_nn, "closed", G_CALLBACK(on_snap_closed), this);
        }

        bool shown = true;
        GError* error = nullptr;
        notify_notification_show(m_nn, &error);
        if (error != NULL)
        {
            g_critical("Unable to show snap decision for '%s': %s", body.c_str(), error->message);
            g_error_free(error);
            shown = false;
        }

        // if we were able to show a popup that requires user response,
        // play the sound in a loop
        if (shown && (m_mode == MODE_SNAP))
            m_sound.reset(new LoopedSound(m_sound_filename, m_sound_volume));

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
    static void on_snap_show(NotifyNotification*, gchar* /*action*/, gpointer gself)
    {
        auto self = static_cast<Self*>(gself);
        self->m_response_value = RESPONSE_SHOW;
        self->m_sound.reset();
    }

    // user clicked 'dismiss'
    static void on_snap_dismiss(NotifyNotification*, gchar* /*action*/, gpointer gself)
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
    ****
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

    typedef enum
    {
        // just a bubble... no actions, no audio
        MODE_BUBBLE,

        // a snap decision popup dialog + audio
        MODE_SNAP
    }
    Mode;

    static Mode get_mode()
    {
        static Mode mode;
        static bool mode_inited = false;

        if (G_UNLIKELY(!mode_inited))
        {
            const auto caps = get_server_caps();

            if (caps.count("actions"))
                mode = MODE_SNAP;
            else
                mode = MODE_BUBBLE;

            mode_inited = true;
        }

        return mode;
    }

    /***
    ****
    ***/

    typedef Popup Self;

    const Appointment m_appointment;
    const std::string m_sound_filename;
    const AlarmVolume m_sound_volume;
    const Mode m_mode;
    std::unique_ptr<LoopedSound> m_sound;
    core::Signal<Response> m_response;
    Response m_response_value = RESPONSE_CLOSE;
    NotifyNotification* m_nn = nullptr;
};

/** 
***  libnotify -- snap decisions
**/

void first_time_init()
{
    static bool inited = false;

    if (G_UNLIKELY(!inited))
    {
        inited = true;

        if(!notify_init("indicator-datetime-service"))
            g_critical("libnotify initialization failed");
    }
}

std::string get_local_filename (const std::string& str)
{
    std::string ret;

    // maybe try it as a file path
    if (ret.empty() && !str.empty())
    {
        auto file = g_file_new_for_path (str.c_str());
        if (g_file_is_native(file) && g_file_query_exists(file,nullptr))
            ret = g_file_get_path(file);
        g_clear_object(&file);
    }

    // maybe try it as a uri
    if (ret.empty() && !str.empty())
    {
        auto file = g_file_new_for_uri (str.c_str());
        if (g_file_is_native(file) && g_file_query_exists(file,nullptr))
            ret = g_file_get_path(file);
        g_clear_object(&file);
    }

    return ret;
}

std::string get_alarm_sound(const Appointment& appointment,
                            const std::shared_ptr<const Settings>& settings)
{
    static const constexpr char* const FALLBACK_AUDIO_FILENAME {"/usr/share/sounds/ubuntu/ringtones/Suru arpeggio.ogg"};

    const std::string candidates[] = { appointment.audio_url,
                                       settings->alarm_sound.get(),
                                       FALLBACK_AUDIO_FILENAME };

    std::string alarm_sound;

    for (const auto& candidate : candidates)
    {
        alarm_sound = get_local_filename (candidate);

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

Snap::Snap(const std::shared_ptr<const Settings>& settings):
    m_settings(settings)
{
    first_time_init();
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
    auto popup = new Popup(appointment,
                           get_alarm_sound(appointment, m_settings),
                           m_settings->alarm_volume.get());
     
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
