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

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <libnotify/notify.h> 

#include "planner-eds.h"
#include "planner-mock.h"
#include "service.h"

/***
****
***/

/* When enabled, new alarms will show up every minute to test snap decisions */
static gboolean test_alarms = FALSE;

static GOptionEntry entries[] = {
  { "test-alarms", '\0', 0, G_OPTION_ARG_NONE, &test_alarms, "Test Alarms", NULL },
  { NULL }
};

static void
on_name_lost (gpointer instance G_GNUC_UNUSED, gpointer loop)
{
  g_message ("exiting: service couldn't acquire or lost ownership of busname");

  if (!test_alarms)
    g_main_loop_quit ((GMainLoop*)loop);
}

int
main (int argc G_GNUC_UNUSED, char ** argv G_GNUC_UNUSED)
{
  GOptionContext * context;
  GError * error;
  IndicatorDatetimePlanner * planner;
  IndicatorDatetimeService * service;
  GMainLoop * loop;

  /* Work around a deadlock in glib's type initialization. It can be
   * removed when https://bugzilla.gnome.org/show_bug.cgi?id=674885 is
   * fixed.
   */
  g_type_ensure (G_TYPE_DBUS_CONNECTION);

  /* boilerplate i18n */
  setlocale (LC_ALL, "");
  bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
  textdomain (GETTEXT_PACKAGE);

  /* init libnotify */
  if (!notify_init ("indicator-datetime-service"))
    g_critical ("libnotify initialization failed");

  /* parse command-line options */
  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_print("option parsing failed: %s\n", error->message);
      return EXIT_FAILURE;
    }

  /* set up the planner */
  if (test_alarms)
    {
      g_message ("Using fake appointment book for testing alarms.");
      planner = indicator_datetime_planner_mock_new ();
    }
  else
    {
      planner = indicator_datetime_planner_eds_new ();
    }

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
