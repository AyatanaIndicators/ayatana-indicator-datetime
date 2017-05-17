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

namespace ayatana {
namespace indicator {
namespace datetime {

/***
****
***/

class TimedatedTimezone::Impl
{
public:

    Impl(TimedatedTimezone& owner, std::string filename):
        m_owner(owner),
        m_filename(filename)
    {
        g_debug("Filename is '%s'", filename.c_str());
        monitor_timezone_property();
    }

    ~Impl()
    {
        clear();
    }

private:

    void clear()
    {
        if (m_connection && m_signal_subscription_id)
        {
            g_dbus_connection_signal_unsubscribe (m_connection, m_signal_subscription_id);
            m_signal_subscription_id = 0;
        }

        g_clear_object(&m_connection);
    }

    static void on_properties_changed (GDBusConnection *connection G_GNUC_UNUSED,
            const gchar *sender_name G_GNUC_UNUSED,
            const gchar *object_path G_GNUC_UNUSED,
            const gchar *interface_name G_GNUC_UNUSED,
            const gchar *signal_name G_GNUC_UNUSED,
            GVariant *parameters,
            gpointer gself)
    {
        auto self = static_cast<Impl*>(gself);
        const char *tz;
        GVariant *changed_properties;
        gchar **invalidated_properties;

        g_variant_get (parameters, "(s@a{sv}^as)", NULL, &changed_properties, &invalidated_properties);

        if (g_variant_lookup(changed_properties, "Timezone", "&s", &tz, NULL))
            self->notify_timezone(tz);
        else if (g_strv_contains (invalidated_properties, "Timezone"))
            self->notify_timezone(self->get_timezone_from_file(self->m_filename));

        g_variant_unref (changed_properties);
        g_strfreev (invalidated_properties);
    }

    void monitor_timezone_property()
    {
        GError *err = nullptr;

        /*
         * There is an unlikely race which happens if there is an activation
         * and timezone change before our match rule is added.
         */
        notify_timezone(get_timezone_from_file(m_filename));

        /*
         * Make sure the bus is around at least until we add the match rules,
         * otherwise things (tests) are sad.
         */
        m_connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM,
                nullptr,
                &err);

        if (err)
        {
            g_warning("Couldn't get bus connection: '%s'", err->message);
            g_error_free(err);
            return;
        }

        m_signal_subscription_id = g_dbus_connection_signal_subscribe(m_connection,
                "org.freedesktop.timedate1",
                "org.freedesktop.DBus.Properties",
                "PropertiesChanged",
                "/org/freedesktop/timedate1",
                NULL, G_DBUS_SIGNAL_FLAGS_NONE,
                on_properties_changed,
                this, nullptr);
    }

    void notify_timezone(std::string new_timezone)
    {
        g_debug("notify_timezone '%s'", new_timezone.c_str());
        if (!new_timezone.empty())
            m_owner.timezone.set(new_timezone);
    }

    std::string get_timezone_from_file(const std::string& filename)
    {
        GError * error;
        GIOChannel * io_channel;
        std::string ret;

        // read through filename line-by-line until we fine a nonempty non-comment line
        error = nullptr;
        io_channel = g_io_channel_new_file(filename.c_str(), "r", &error);
        if (error == nullptr)
        {
            auto line = g_string_new(nullptr);

            while(ret.empty())
            {
                const auto io_status = g_io_channel_read_line_string(io_channel, line, nullptr, &error);
                if ((io_status == G_IO_STATUS_EOF) || (io_status == G_IO_STATUS_ERROR))
                    break;
                if (error != nullptr)
                    break;

                g_strstrip(line->str);

                if (!line->len) // skip empty lines
                    continue;

                if (*line->str=='#') // skip comments
                    continue;

                ret = line->str;
            }

            g_string_free(line, true);
        } else
            /* Default to UTC */
            ret = "Etc/Utc";

        if (io_channel != nullptr)
        {
            g_io_channel_shutdown(io_channel, false, nullptr);
            g_io_channel_unref(io_channel);
        }

        if (error != nullptr)
        {
            g_warning("%s Unable to read timezone file '%s': %s", G_STRLOC, filename.c_str(), error->message);
            g_error_free(error);
        }

        return ret;
    }

    /***
    ****
    ***/

    TimedatedTimezone & m_owner;
    GDBusConnection *m_connection = nullptr;
    unsigned long m_signal_subscription_id = 0;
    std::string m_filename;
};

/***
****
***/

TimedatedTimezone::TimedatedTimezone(std::string filename):
    impl(new Impl{*this, filename})
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
} // namespace ayatana
