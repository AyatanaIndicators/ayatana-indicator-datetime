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
#include <datetime/utils.h> // generate_full_format_string_at_time()

#include <url-dispatcher.h>

#include <libnotify/notify.h>

#include <glib/gi18n.h>
#include <glib.h>

namespace unity {
namespace indicator {
namespace datetime {

/***
****
***/

namespace
{

void dispatch_alarm_url(const Appointment& appointment)
{
  g_return_if_fail(!appointment.has_alarms);

  const auto fmt = appointment.begin.format("%F %T");
  g_debug("dispatching url \"%s\" for appointment \"%s\", which begins at %s",
          appointment.url.c_str(),
          appointment.summary.c_str(),
          fmt.c_str());

  url_dispatch_send(appointment.url.c_str(), nullptr, nullptr);
}

void on_snap_decided(NotifyNotification  * /*notification*/,
                     char                *   action,
                     gpointer                gurl)
{
    g_debug("%s: %s", G_STRFUNC, action);

    if (!g_strcmp0(action, "show"))
    {
        const auto url = static_cast<const gchar*>(gurl);
        g_debug("dispatching url '%s'", url);
        url_dispatch_send(url, nullptr, nullptr);
    }
}

} // unnamed namespace

/***
****
***/

Snap::Snap()
{
}

Snap::~Snap()
{
}

void Snap::operator()(const Appointment& appointment)
{
    if (!appointment.has_alarms)
        return;

    auto timestr = generate_full_format_string_at_time (appointment.begin.get(), nullptr, nullptr);
    auto title = g_strdup_printf(_("Alarm %s"), timestr);
    const auto body = appointment.summary;
    const gchar* icon_name = "alarm-clock";
    g_debug("creating a snap decision with title '%s', body '%s', icon '%s'", title, body.c_str(), icon_name);

    auto nn = notify_notification_new(title, body.c_str(), icon_name);
    notify_notification_set_hint_string(nn, "x-canonical-snap-decisions", "true");
    notify_notification_set_hint_string(nn, "x-canonical-private-button-tint", "true");
    notify_notification_add_action(nn, "show", _("Show"), on_snap_decided, g_strdup(appointment.url.c_str()), g_free);
    notify_notification_add_action(nn, "dismiss", _("Dismiss"), on_snap_decided, nullptr, nullptr);

    GError * error = nullptr;
    notify_notification_show(nn, &error);
    if (error != NULL)
    {
        g_warning("Unable to show alarm '%s' popup: %s", body.c_str(), error->message);
        g_error_free(error);
        dispatch_alarm_url(appointment);
    }

    g_free(title);
    g_free(timestr);
}

/***
****
***/

} // namespace datetime
} // namespace indicator
} // namespace unity
