/*
 * Copyright 2013 Canonical Ltd.
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

#include <datetime/timezone-timedated.h>

#include <gio/gio.h>

#include <cerrno>
#include <cstdlib>

namespace unity {
namespace indicator {
namespace datetime {

/***
****
***/

class TimedatedTimezone::Impl
{
public:

    Impl(TimedatedTimezone& owner):
        m_owner(owner),
        m_loop(g_main_loop_new(nullptr, FALSE))
    {
        monitor_timezone_property();
    }

    ~Impl()
    {
        clear();
    }

private:

    void clear()
    {
        if (m_bus_watch_id)
        {
            g_bus_unwatch_name (m_bus_watch_id);
            m_bus_watch_id = 0;
        }

        if (m_properties_changed_id)
        {
            g_signal_handler_disconnect(m_proxy, m_properties_changed_id);
            m_properties_changed_id = 0;
        }

        if (m_timeout_id)
        {
            g_source_remove(m_timeout_id);
            m_timeout_id = 0;
        }

        g_clear_object(&m_proxy);
        g_clear_pointer(&m_loop, g_main_loop_unref);
    }

    static void on_properties_changed(GDBusProxy *proxy G_GNUC_UNUSED,
            GVariant *changed_properties /* a{sv} */,
            GStrv invalidated_properties G_GNUC_UNUSED,
            gpointer gself)
    {
        auto self = static_cast<Impl*>(gself);
        char *tz;

        if (g_variant_lookup(changed_properties, "Timezone", "s", &tz, NULL))
        {
            g_debug("on_properties_changed: got timezone '%s'", tz);
            self->notify_timezone(tz);
            g_free (tz);
        }
    }

    static void on_proxy_ready(GObject *object G_GNUC_UNUSED,
            GAsyncResult *res,
            gpointer gself)
    {
        auto self = static_cast<Impl*>(gself);
        GError *error = nullptr;
        self->m_proxy = g_dbus_proxy_new_finish(res, &error);

        if (error)
        {
            g_warning ("Couldn't create proxy to read timezone: %s", error->message);
            goto out;
        }

        /* Read the property */
        GVariant *prop;
        prop = g_dbus_proxy_get_cached_property(self->m_proxy, "Timezone");

        if (!prop || !g_variant_is_of_type(prop, G_VARIANT_TYPE_STRING))
        {
            g_warning("Couldn't read the Timezone property, defaulting to Etc/Utc");
            self->notify_timezone("Etc/Utc");
            goto out;
        }

        const gchar *tz;
        tz = g_variant_get_string(prop, nullptr);

        self->notify_timezone(tz);

        self->m_properties_changed_id = g_signal_connect(self->m_proxy,
                "g-properties-changed",
                (GCallback) on_properties_changed,
                gself);

out:
        g_clear_pointer(&error, g_error_free);
        g_clear_pointer(&prop, g_variant_unref);
        if (self->m_loop && g_main_loop_is_running(self->m_loop))
            g_main_loop_quit(self->m_loop);

        if (self->m_timeout_id)
        {
            g_source_remove(self->m_timeout_id);
            self->m_timeout_id = 0;
        }
    }

    static void on_name_appeared(GDBusConnection *connection,
            const gchar *name,
            const gchar *name_owner G_GNUC_UNUSED,
            gpointer gself G_GNUC_UNUSED)
    {
        g_debug ("timedate1 appeared");
        g_dbus_proxy_new(connection,
                G_DBUS_PROXY_FLAGS_NONE,
                NULL,
                name,
                "/org/freedesktop/timedate1",
                "org.freedesktop.timedate1",
                nullptr,
                on_proxy_ready,
                gself);
    }

    static gboolean quit_loop(gpointer gself)
    {
        auto self = static_cast<Impl*>(gself);

        g_warning("Timed out when getting initial value of timezone, defaulting to UTC");
        self->notify_timezone("Etc/Utc");

        g_main_loop_quit(self->m_loop);

        self->m_timeout_id = 0;

        return G_SOURCE_REMOVE;
    }

    static void on_name_vanished(GDBusConnection *connection G_GNUC_UNUSED,
            const gchar *name G_GNUC_UNUSED,
            gpointer gself)
    {
        auto self = static_cast<Impl*>(gself);
        g_debug ("timedate1 vanished");

        g_signal_handler_disconnect(self->m_proxy,
                self->m_properties_changed_id);
        self->m_properties_changed_id = 0;
        g_clear_object(&self->m_proxy);
        g_clear_pointer(&self->m_proxy, g_main_loop_unref);
    }

    void monitor_timezone_property()
    {
        m_bus_watch_id = g_bus_watch_name (G_BUS_TYPE_SYSTEM,
                "org.freedesktop.timedate1",
                G_BUS_NAME_WATCHER_FLAGS_AUTO_START,
                on_name_appeared,
                on_name_vanished,
                this,
                nullptr);

        /* Incase something breaks, we don't want to hang */
        m_timeout_id = g_timeout_add(500, quit_loop, this);

        /* We need to block until we've read the tz once */
        g_main_loop_run(m_loop);
    }

    void notify_timezone(std::string new_timezone)
    {
        if (!new_timezone.empty())
            m_owner.timezone.set(new_timezone);
    }

    /***
    ****
    ***/

    TimedatedTimezone & m_owner;
    unsigned long m_properties_changed_id = 0;
    unsigned long m_bus_watch_id = 0;
    unsigned long m_timeout_id = 0;
    GDBusProxy *m_proxy = nullptr;
    GMainLoop *m_loop = nullptr;
};

/***
****
***/

TimedatedTimezone::TimedatedTimezone():
    impl(new Impl{*this})
{
}

TimedatedTimezone::~TimedatedTimezone()
{
}

/***
****
***/

} // namespace datetime
} // namespace indicator
} // namespace unity
