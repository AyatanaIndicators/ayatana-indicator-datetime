/*
 * Copyright 2013 Canonical Ltd.
 *
 * Authors:
 *   Charles Kerr <charles.kerr@canonical.com>
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
 */

#include "config.h"

#include <locale.h>
#include <stdlib.h> /* exit() */
#include <stdio.h>

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <libnotify/notify.h> 

#include "planner-eds.h"
#include "planner-mock.h"
#include "service.h"

/***
****
***/

static void
on_name_lost (gpointer instance G_GNUC_UNUSED, gpointer loop G_GNUC_UNUSED)
{
  g_message ("exiting: service couldn't acquire or lost ownership of busname");
  g_main_loop_quit ((GMainLoop*)loop);
}

static void
action_ok (NotifyNotification *notification  G_GNUC_UNUSED,
           char               *action,
           gpointer            gurl)
{
  const char * url = gurl;
  g_message ("'%s' clicked for snap decision; url is '%s'", action, url);
}

static void
show_snap_decision (void)
{
  const gchar * title = "Title";
  const gchar * body = "Body";
  const gchar * icon_name = "alarm-clock";
  NotifyNotification * nn;
  GError * error;

  g_debug ("creating a snap decision with title '%s', body '%s', icon '%s'",
           title, body, icon_name);

  nn = notify_notification_new (title, body, icon_name);
  notify_notification_set_hint (nn, "x-canonical-snap-decisions",
                                g_variant_new_boolean(TRUE));
  notify_notification_set_hint (nn, "x-canonical-private-button-tint",
                                g_variant_new_boolean(TRUE));
  notify_notification_add_action (nn, "action_accept", _("OK"),
                                  action_ok, g_strdup("hello world"), g_free);

  g_message ("showing notification %p", nn);
  error = NULL;
  notify_notification_show (nn, &error);
  if (error != NULL)
    {
      g_warning ("Unable to show alarm '%s' popup: %s", body, error->message);
      g_error_free (error);
    }
}

int
main (int argc G_GNUC_UNUSED, char ** argv G_GNUC_UNUSED)
{
  IndicatorDatetimePlanner * planner;
  IndicatorDatetimeService * service;
  GMainLoop * loop;

  /* boilerplate i18n */
  setlocale (LC_ALL, "");
  bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
  textdomain (GETTEXT_PACKAGE);

  /* init libnotify */
  if (!notify_init ("indicator-datetime-service"))
    g_critical ("libnotify initialization failed");

  /* set up the planner */
#ifdef TEST_MODE
  g_warning ("Using fake appointment book for testing! "
             "Probably shouldn't merge this to trunk.");
  planner = indicator_datetime_planner_mock_new ();
#else
  planner = indicator_datetime_planner_eds_new ();
#endif

  show_snap_decision ();

  /* run */
  service = indicator_datetime_service_new (planner);
  loop = g_main_loop_new (NULL, FALSE);
  g_signal_connect (service, INDICATOR_DATETIME_SERVICE_SIGNAL_NAME_LOST,
                    G_CALLBACK(on_name_lost), loop);
  g_main_loop_run (loop);

  /* cleanup */
  g_main_loop_unref (loop);
  g_object_unref (service);
  g_object_unref (planner);
  return 0;
}
