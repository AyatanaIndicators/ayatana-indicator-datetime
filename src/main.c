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

#include "service.h"

/***
****
***/

static gboolean replace = FALSE;

static void
parse_command_line (int * argc, char *** argv)
{
  GError * error;
  GOptionContext * option_context;

  static GOptionEntry entries[] =
  {
    { "replace", 'r', 0, G_OPTION_ARG_NONE, &replace, "Replace the currently-running service", NULL },
    { NULL }
  };

  error = NULL;
  option_context = g_option_context_new ("- indicator-datetime service");
  g_option_context_add_main_entries (option_context, entries, GETTEXT_PACKAGE);
  if (!g_option_context_parse (option_context, argc, argv, &error))
    {
      g_print ("option parsing failed: %s\n", error->message);
      g_error_free (error);
      exit (EXIT_FAILURE);
    }

  g_option_context_free (option_context);
}

/***
****
***/

static void
on_name_lost (gpointer instance G_GNUC_UNUSED, gpointer loop)
{
  g_message ("exiting: service couldn't acquire or lost ownership of busname");
  g_main_loop_quit ((GMainLoop*)loop);
}

int
main (int argc, char ** argv)
{
  GMainLoop * loop;
  IndicatorDatetimeService * service;

  /* boilerplate i18n */
  setlocale (LC_ALL, "");
  bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
  textdomain (GETTEXT_PACKAGE);

  parse_command_line (&argc, &argv);

  /* run */
  service = indicator_datetime_service_new (replace);
  loop = g_main_loop_new (NULL, FALSE);
  g_signal_connect (service, INDICATOR_DATETIME_SERVICE_SIGNAL_NAME_LOST,
                    G_CALLBACK(on_name_lost), loop);
  g_main_loop_run (loop);

  /* cleanup */
  g_clear_object (&service);
  g_main_loop_unref (loop);
  return 0;
}
