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

#define TEST_MODE

/***
****
***/

static void
on_name_lost (gpointer instance G_GNUC_UNUSED, gpointer loop)
{
  g_message ("exiting: service couldn't acquire or lost ownership of busname");
  g_main_loop_quit ((GMainLoop*)loop);
}

#ifdef TEST_MODE
static void
log_handler (const gchar    * log_domain,
             GLogLevelFlags   log_level,
             const gchar    * message,
             gpointer         fp)
{
  fprintf (fp, "%s %d %s\n", log_domain, (int)log_level, message);
  fflush (fp);
}
#endif

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
  g_warning ("Using fake appointment book for testing! Probably shouldn't merge this to trunk.");
  FILE * fp = fopen ("/tmp/indicator-datetime-log.txt", "w+");
  g_log_set_handler ("Indicator-Datetime", G_LOG_LEVEL_MASK, log_handler, fp);
  planner = indicator_datetime_planner_mock_new ();
#else
  planner = indicator_datetime_planner_eds_new ();
#endif

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
#ifdef TEST_MODE
  fclose (fp);
#endif
  return 0;
}
