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

#include <datetime/timezone-file.h>

namespace unity {
namespace indicator {
namespace datetime {

void
FileTimezone::clear()
{
    if (monitor_handler_id_)
        g_signal_handler_disconnect(monitor_, monitor_handler_id_);

    g_clear_object (&monitor_);

    filename_.clear();
}

void
FileTimezone::setFilename(const std::string& filename)
{
    clear();

    filename_ = filename;

    auto file = g_file_new_for_path(filename.c_str());
    GError * err = nullptr;
    monitor_ = g_file_monitor_file(file, G_FILE_MONITOR_NONE, nullptr, &err);
    g_object_unref(file);
    if (err)
      {
        g_warning("%s Unable to monitor timezone file '%s': %s", G_STRLOC, TIMEZONE_FILE, err->message);
        g_error_free(err);
      }
    else
      {
        monitor_handler_id_ = g_signal_connect_swapped(monitor_, "changed", G_CALLBACK(onFileChanged), this);
        g_debug("%s Monitoring timezone file '%s'", G_STRLOC, filename.c_str());
      }

    reload();
}

void
FileTimezone::onFileChanged(gpointer gself)
{
    static_cast<FileTimezone*>(gself)->reload();
}

void
FileTimezone::reload()
{
    GError * err = nullptr;
    gchar * str = nullptr;

    if (!g_file_get_contents(filename_.c_str(), &str, nullptr, &err))
    {
        g_warning("%s Unable to read timezone file '%s': %s", G_STRLOC, filename_.c_str(), err->message);
        g_error_free(err);
    }
    else
    {
        g_strstrip(str);
        timezone.set(str);
        g_free(str);
    }
}

} // namespace datetime
} // namespace indicator
} // namespace unity
