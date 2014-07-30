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
#include <datetime/dbus-shared.h>
#include <datetime/formatter.h>
#include <datetime/snap.h>

#include <core/signal.h>

#include <gst/gst.h>
#include <libnotify/notify.h>

#include <glib/gi18n.h>
#include <glib.h>

#include <chrono>
#include <limits>
#include <mutex> // std::call_once()
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

static constexpr char const * APP_NAME = {"indicator-datetime-service"};

/**
 * Plays a sound, possibly looping.
 */
class Sound
{
    typedef Sound Self;

public:

    Sound(const std::shared_ptr<Clock>& clock,
          const std::string& uri,
          unsigned int volume,
          unsigned int duration_minutes,
          bool loop):
        m_clock(clock),
        m_uri(uri),
        m_volume(volume),
        m_loop(loop),
        m_loop_end_time(clock->localtime().add_full(0, 0, 0, 0, (int)duration_minutes, 0.0))
    {
        // init GST once
	static std::once_flag once;
        std::call_once(once, [](){
            GError* error = nullptr;
            gst_init_check (nullptr, nullptr, &error);
            if (error)
            {
                g_critical("Unable to play alarm sound: %s", error->message);
                g_error_free(error);
            }
        });

        if (m_loop)
        {
            g_debug("Looping '%s' until cutoff time %s",
                    m_uri.c_str(),
                    m_loop_end_time.format("%F %T").c_str());
        }
        else
        {
            g_debug("Playing '%s' once", m_uri.c_str());
        }

        m_play = gst_element_factory_make("playbin", "play");

        auto bus = gst_pipeline_get_bus(GST_PIPELINE(m_play));
        m_watch_source = gst_bus_add_watch(bus, bus_callback, this);
        gst_object_unref(bus);

        play();
    }

    ~Sound()
    {
        stop();

        g_source_remove(m_watch_source);

        if (m_play != nullptr)
        {
            gst_element_set_state (m_play, GST_STATE_NULL);
            g_clear_pointer (&m_play, gst_object_unref);
        }
    }

private:

    void stop()
    {
        if (m_play != nullptr)
        {
            gst_element_set_state (m_play, GST_STATE_PAUSED);
        }
    }

    void play()
    {
       g_return_if_fail(m_play != nullptr);

       g_object_set(G_OBJECT (m_play), "uri", m_uri.c_str(),
                                       "volume", get_volume(),
                                       nullptr);
       gst_element_set_state (m_play, GST_STATE_PLAYING);
    }

    // convert settings range [1..100] to gst playbin's range is [0...1.0]
    gdouble get_volume() const
    {
        constexpr int in_range_lo = 1;
        constexpr int in_range_hi = 100;
        const double in = CLAMP(m_volume, in_range_lo, in_range_hi);
        const double pct = (in - in_range_lo) / (in_range_hi - in_range_lo);

        constexpr double out_range_lo = 0.0; 
        constexpr double out_range_hi = 1.0; 
        return out_range_lo + (pct * (out_range_hi - out_range_lo));
    }

    static gboolean bus_callback(GstBus*, GstMessage* msg, gpointer gself)
    {
        auto self = static_cast<Sound*>(gself);

        if ((GST_MESSAGE_TYPE(msg) == GST_MESSAGE_EOS) &&
            (self->m_loop) &&
            (self->m_clock->localtime() < self->m_loop_end_time))
        {
            gst_element_seek(self->m_play,
                             1.0,
                             GST_FORMAT_TIME,
                             GST_SEEK_FLAG_FLUSH,
                             GST_SEEK_TYPE_SET,
                             0,
                             GST_SEEK_TYPE_NONE,
                             (gint64)GST_CLOCK_TIME_NONE);
        }

        return G_SOURCE_CONTINUE; // keep listening
    }

    /***
    ****
    ***/

    const std::shared_ptr<Clock> m_clock;
    const std::string m_uri;
    const unsigned int m_volume;
    const bool m_loop;
    const DateTime m_loop_end_time;
    guint m_watch_source = 0;
    GstElement* m_play = nullptr;
};

class SoundBuilder
{
public:
    void set_clock(const std::shared_ptr<Clock>& c) {m_clock = c;}
    void set_uri(const std::string& uri) {m_uri = uri;}
    void set_volume(const unsigned int v) {m_volume = v;}
    void set_duration_minutes(unsigned int i) {m_duration_minutes=i;}
    unsigned int duration_minutes() const {return m_duration_minutes;}
    void set_looping(bool b) {m_looping=b;}

    Sound* operator()() {
        return new Sound (m_clock,
                          m_uri,
                          m_volume,
                          m_duration_minutes,
                          m_looping);
    }

private:
    std::shared_ptr<Clock> m_clock;
    std::string m_uri;
    unsigned int m_volume = 50;
    unsigned int m_duration_minutes = 30;
    bool m_looping = true;
};

/** 
***  libnotify -- snap decisions
**/

std::string get_alarm_uri(const Appointment& appointment,
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

int32_t n_existing_snaps = 0;

} // unnamed namespace

/**
 * A popup notification (with optional sound)
 * that emits a Response signal when done.
 */
class Snap::Popup
{
public:

    Popup(const Appointment& appointment, const SoundBuilder& sound_builder):
        m_appointment(appointment),
        m_interactive(get_interactive()),
        m_sound_builder(sound_builder),
        m_cancellable(g_cancellable_new())
    {
        g_bus_get (G_BUS_TYPE_SYSTEM, m_cancellable, on_system_bus_ready, this);

        show();
    }

    ~Popup()
    {
        if (m_cancellable != nullptr)
        {
            g_cancellable_cancel (m_cancellable);
            g_clear_object (&m_cancellable);
        }

        if (m_system_bus != nullptr)
        {
            unforce_awake ();
            unforce_screen ();
            g_clear_object (&m_system_bus);
        } 

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
            const auto duration = std::chrono::minutes(m_sound_builder.duration_minutes());

            notify_notification_set_hint(m_nn, HINT_SNAP,
                                         g_variant_new_boolean(true));
            notify_notification_set_hint(m_nn, HINT_TINT,
                                         g_variant_new_boolean(true));
            notify_notification_set_hint(m_nn, HINT_TIMEOUT,
                                         g_variant_new_int32(std::chrono::duration_cast<std::chrono::milliseconds>(duration).count()));
            notify_notification_set_hint(m_nn, HINT_NONSHAPEDICON,
                                         g_variant_new_boolean(true));

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

	static std::once_flag once;
        std::call_once(once, [](){
            interactive = get_server_caps().count("actions") != 0;
        });

        return interactive;
    }

    /***
    ****
    ***/

    static void on_system_bus_ready (GObject *, GAsyncResult *res, gpointer gself)
    {
        GError * error;
        GDBusConnection * system_bus;

        error = nullptr;
        system_bus = g_bus_get_finish (res, &error);
        if (error != nullptr)
        {
            if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                g_warning ("Unable to get bus: %s", error->message);

            g_error_free (error);
        }
        else if (system_bus != nullptr)
        {
            auto self = static_cast<Popup*>(gself);

            self->m_system_bus = G_DBUS_CONNECTION (g_object_ref (system_bus));

            // ask powerd to keep the system awake
            static constexpr int32_t POWERD_SYS_STATE_ACTIVE = 1;
            g_dbus_connection_call (system_bus,
                                    BUS_POWERD_NAME,
                                    BUS_POWERD_PATH,
                                    BUS_POWERD_INTERFACE,
                                    "requestSysState",
                                    g_variant_new("(si)", APP_NAME, POWERD_SYS_STATE_ACTIVE),
                                    G_VARIANT_TYPE("(s)"),
                                    G_DBUS_CALL_FLAGS_NONE,
                                    -1,
                                    self->m_cancellable,
                                    on_force_awake_response,
                                    self);

            // ask unity-system-compositor to turn on the screen
            g_dbus_connection_call (system_bus,
                                    BUS_SCREEN_NAME,
                                    BUS_SCREEN_PATH,
                                    BUS_SCREEN_INTERFACE,
                                    "keepDisplayOn",
                                    nullptr,
                                    G_VARIANT_TYPE("(i)"),
                                    G_DBUS_CALL_FLAGS_NONE,
                                    -1,
                                    self->m_cancellable,
                                    on_force_screen_response,
                                    self);

            g_object_unref (system_bus);
        }
    }

    static void on_force_awake_response (GObject      * connection,
                                         GAsyncResult * res,
                                         gpointer       gself)
    {
        GError * error;
        GVariant * args;

        error = nullptr;
        args = g_dbus_connection_call_finish (G_DBUS_CONNECTION(connection), res, &error);
        if (error != nullptr)
        {
            if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                g_warning ("Unable to inhibit sleep: %s", error->message);

            g_error_free (error);
        }
        else
        {
            auto self = static_cast<Popup*>(gself);

            g_clear_pointer (&self->m_awake_cookie, g_free);
            g_variant_get (args, "(s)", &self->m_awake_cookie);
            g_debug ("m_awake_cookie is now '%s'", self->m_awake_cookie);
 
            g_variant_unref (args);
        }
    }

    static void on_force_screen_response (GObject      * connection,
                                          GAsyncResult * res,
                                          gpointer       gself)
    {
        GError * error;
        GVariant * args;

        error = nullptr;
        args = g_dbus_connection_call_finish (G_DBUS_CONNECTION(connection), res, &error);
        if (error != nullptr)
        {
            if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                g_warning ("Unable to turn on the screen: %s", error->message);

            g_error_free (error);
        }
        else
        {
            auto self = static_cast<Popup*>(gself);

            self->m_screen_cookie = NO_SCREEN_COOKIE;
            g_variant_get (args, "(i)", &self->m_screen_cookie);
            g_debug ("m_screen_cookie is now '%d'", self->m_screen_cookie);
 
            g_variant_unref (args);
        }
    }

    void unforce_awake ()
    {
        g_return_if_fail (G_IS_DBUS_CONNECTION(m_system_bus));

        if (m_awake_cookie != nullptr)
        {
            g_dbus_connection_call (m_system_bus,
                                    BUS_POWERD_NAME,
                                    BUS_POWERD_PATH,
                                    BUS_POWERD_INTERFACE,
                                    "clearSysState",
                                    g_variant_new("(s)", m_awake_cookie),
                                    nullptr,
                                    G_DBUS_CALL_FLAGS_NONE,
                                    -1,
                                    nullptr,
                                    nullptr,
                                    nullptr);

            g_clear_pointer (&m_awake_cookie, g_free);
        }
    }

    void unforce_screen ()
    {
        g_return_if_fail (G_IS_DBUS_CONNECTION(m_system_bus));

        if (m_screen_cookie != NO_SCREEN_COOKIE)
        {
            g_dbus_connection_call (m_system_bus,
                                    BUS_SCREEN_NAME,
                                    BUS_SCREEN_PATH,
                                    BUS_SCREEN_INTERFACE,
                                    "removeDisplayOnRequest",
                                    g_variant_new("(i)", m_screen_cookie),
                                    nullptr,
                                    G_DBUS_CALL_FLAGS_NONE,
                                    -1,
                                    nullptr,
                                    nullptr,
                                    nullptr);

            m_screen_cookie = NO_SCREEN_COOKIE;
        }
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
    GCancellable * m_cancellable = nullptr;
    GDBusConnection * m_system_bus = nullptr;
    char * m_awake_cookie = nullptr;
    int32_t m_screen_cookie = NO_SCREEN_COOKIE;

    static constexpr int32_t NO_SCREEN_COOKIE { std::numeric_limits<int32_t>::min() };

    static constexpr char const * HINT_SNAP {"x-canonical-snap-decisions"};
    static constexpr char const * HINT_TINT {"x-canonical-private-button-tint"};
    static constexpr char const * HINT_TIMEOUT {"x-canonical-snap-decisions-timeout"};
    static constexpr char const * HINT_NONSHAPEDICON {"x-canonical-non-shaped-icon"};
};

/***
****
***/

Snap::Snap(const std::shared_ptr<Clock>& clock,
           const std::shared_ptr<const Settings>& settings):
    m_clock(clock),
    m_settings(settings)
{
    if (!n_existing_snaps++ && !notify_init(APP_NAME))
        g_critical("libnotify initialization failed");
}

Snap::~Snap()
{
    for (auto popup : m_pending)
        delete popup;

    if (!--n_existing_snaps)
        notify_uninit();
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
    sound_builder.set_uri(get_alarm_uri(appointment, m_settings));
    sound_builder.set_volume(m_settings->alarm_volume.get());
    sound_builder.set_clock(m_clock);
    sound_builder.set_duration_minutes(m_settings->alarm_duration.get());
    auto popup = new Popup(appointment, sound_builder);

    m_pending.insert(popup);
     
    // listen for it to finish...
    popup->response().connect([this,
                               appointment,
                               show,
                               dismiss,
                               popup](Popup::Response response){

        m_pending.erase(popup);
     
        // we can't delete the Popup inside its response() signal handler
        // because core::signal deadlocks, so push that to an idle func
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
