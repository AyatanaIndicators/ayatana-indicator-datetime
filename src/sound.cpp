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

#include <notifications/sound.h>

#include <gst/gst.h>

#include <mutex> // std::call_once()

namespace unity {
namespace indicator {
namespace notifications {

/***
****
***/

/**
 * Plays a sound, possibly looping.
 */
class Sound::Impl
{
public:

    Impl(const std::string& uri,
         unsigned int volume,
         bool loop):
        m_uri(uri),
        m_volume(volume),
        m_loop(loop)
    {
        // init GST once
        static std::once_flag once;
        std::call_once(once, [](){
            GError* error = nullptr;
            if (!gst_init_check (nullptr, nullptr, &error))
            {
                g_critical("Unable to play alarm sound: %s", error->message);
                g_error_free(error);
            }
        });

        m_play = gst_element_factory_make("playbin", "play");

        auto bus = gst_pipeline_get_bus(GST_PIPELINE(m_play));
        m_watch_source = gst_bus_add_watch(bus, bus_callback, this);
        gst_object_unref(bus);

        g_debug("Playing '%s'", m_uri.c_str());
        g_object_set(G_OBJECT (m_play), "uri", m_uri.c_str(),
                                        "volume", get_volume(),
                                        nullptr);
        gst_element_set_state (m_play, GST_STATE_PLAYING);
    }

    ~Impl()
    {
        g_source_remove(m_watch_source);

        if (m_play != nullptr)
        {
            gst_element_set_state (m_play, GST_STATE_NULL);
            g_clear_pointer (&m_play, gst_object_unref);
        }
    }

private:

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
        auto self = static_cast<Impl*>(gself);

        if ((GST_MESSAGE_TYPE(msg) == GST_MESSAGE_EOS) && (self->m_loop))
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

    const std::string m_uri;
    const unsigned int m_volume;
    const bool m_loop;
    guint m_watch_source = 0;
    GstElement* m_play = nullptr;
};

Sound::Sound(const std::string& uri, unsigned int volume, bool loop):
  impl (new Impl(uri, volume, loop))
{
}

Sound::~Sound()
{
}

/***
****
***/

} // namespace notifications
} // namespace indicator
} // namespace unity
