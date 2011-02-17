/* -*- Mode: C; coding: utf-8; indent-tabs-mode: nil; tab-width: 2 -*-

A dialog for setting time and date preferences.

Copyright 2011 Canonical Ltd.

Authors:
    Ted Gould <ted@canonical.com>
    Michael Terry <michael.terry@canonical.com>

This program is free software: you can redistribute it and/or modify it 
under the terms of the GNU General Public License version 3, as published 
by the Free Software Foundation.

This program is distributed in the hope that it will be useful, but 
WITHOUT ANY WARRANTY; without even the implied warranties of 
MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR 
PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along 
with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <libintl.h>
#include <locale.h>
#include <gtk/gtk.h>
#include <unique/unique.h>

#include "settings-shared.h"
#include "utils.h"

#define DATETIME_DIALOG_UI_FILE PKGDATADIR "/datetime-dialog.ui"

/* Turns the boolean property into a string gsettings */
static GVariant *
bind_hours_set (const GValue * value, const GVariantType * type, gpointer user_data)
{
  const gchar * output = NULL;
  gboolean is_12hour_button = (gboolean)GPOINTER_TO_INT(user_data);

	if (g_value_get_boolean(value)) {
    /* Only do anything if we're setting active = true */
		output = is_12hour_button ? "12-hour" : "24-hour";
	} else {
    return NULL;
  }

	return g_variant_new_string (output);
}

/* Turns a string gsettings into a boolean property */
static gboolean
bind_hours_get (GValue * value, GVariant * variant, gpointer user_data)
{
	const gchar * str = g_variant_get_string(variant, NULL);
	gboolean output = FALSE;
  gboolean is_12hour_button = (gboolean)GPOINTER_TO_INT(user_data);

	if (g_strcmp0(str, "locale-default") == 0) {
		output = (is_12hour_button == is_locale_12h ());
	} else if (g_strcmp0(str, "12-hour") == 0) {
		output = is_12hour_button;
	} else if (g_strcmp0(str, "24-hour") == 0) {
		output = !is_12hour_button;
	} else {
		return FALSE;
	}

	g_value_set_boolean (value, output);
	return TRUE;
}

static void
widget_dependency_cb (GtkWidget * parent, GParamSpec *pspec, GtkWidget * dependent)
{
  gboolean active, sensitive;
  g_object_get (G_OBJECT (parent),
                "active", &active,
                "sensitive", &sensitive, NULL);
  gtk_widget_set_sensitive (dependent, active && sensitive);
}

static void
add_widget_dependency (GtkWidget * parent, GtkWidget * dependent)
{
  g_signal_connect (parent, "notify::active", G_CALLBACK(widget_dependency_cb),
                    dependent);
  g_signal_connect (parent, "notify::sensitive", G_CALLBACK(widget_dependency_cb),
                    dependent);
  widget_dependency_cb (parent, NULL, dependent);
}

static GtkWidget *
create_dialog (void)
{
  GError * error = NULL;

  GtkBuilder * builder = gtk_builder_new ();
  gtk_builder_add_from_file (builder, DATETIME_DIALOG_UI_FILE, &error);
  if (error != NULL) {
    /* We have to abort, we can't continue without the ui file */
    g_error ("Could not load ui file %s: %s", DATETIME_DIALOG_UI_FILE, error->message);
    g_error_free (error);
    return NULL;
  }

  gtk_builder_set_translation_domain (builder, GETTEXT_PACKAGE);

	GSettings * conf = g_settings_new (SETTINGS_INTERFACE);

#define WIG(name) GTK_WIDGET (gtk_builder_get_object (builder, name))

  /* Set up settings bindings */

  g_settings_bind (conf, SETTINGS_SHOW_CLOCK_S, WIG ("showClockCheck"),
                   "active", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (conf, SETTINGS_SHOW_DAY_S, WIG ("showWeekdayCheck"),
                   "active", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (conf, SETTINGS_SHOW_DATE_S, WIG ("showDateTimeCheck"),
                   "active", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (conf, SETTINGS_SHOW_SECONDS_S, WIG ("showSecondsCheck"),
                   "active", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind_with_mapping (conf, SETTINGS_TIME_FORMAT_S,
                                WIG ("show12HourRadio"), "active",
                                G_SETTINGS_BIND_DEFAULT,
                                bind_hours_get, bind_hours_set,
                                GINT_TO_POINTER(TRUE), NULL);
  g_settings_bind_with_mapping (conf, SETTINGS_TIME_FORMAT_S,
                                WIG ("show24HourRadio"), "active",
                                G_SETTINGS_BIND_DEFAULT,
                                bind_hours_get, bind_hours_set,
                                GINT_TO_POINTER(FALSE), NULL);
  g_settings_bind (conf, SETTINGS_SHOW_CALENDAR_S, WIG ("showCalendarCheck"),
                   "active", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (conf, SETTINGS_SHOW_WEEK_NUMBERS_S, WIG ("includeWeekNumbersCheck"),
                   "active", G_SETTINGS_BIND_DEFAULT);
  /*g_settings_bind_(conf, SETTINGS_WEEK_BEGINS_SUNDAY_S, WIG ("startOnSundayRadio"),
                   "active", G_SETTINGS_BIND_DEFAULT);*/
  g_settings_bind (conf, SETTINGS_SHOW_EVENTS_S, WIG ("showEventsCheck"),
                   "active", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (conf, SETTINGS_SHOW_LOCATIONS_S, WIG ("showLocationsCheck"),
                   "active", G_SETTINGS_BIND_DEFAULT);

  /* Set up sensitivities */
  add_widget_dependency (WIG ("showCalendarCheck"), WIG ("calendarOptions"));
  add_widget_dependency (WIG ("showClockCheck"), WIG ("clockOptions"));
  add_widget_dependency (WIG ("showLocationsCheck"), WIG ("locationsButton"));
  add_widget_dependency (WIG ("manualTimeRadio"), WIG ("manualOptions"));

  /* Hacky proxy test for whether evolution-data-server is installed */
  gchar * evo_path = g_find_program_in_path ("evolution");
  gtk_widget_set_sensitive (WIG ("showEventsCheck"), (evo_path != NULL));
  g_free (evo_path);

  GtkWidget * dlg = WIG ("timeDateDialog");

#undef WIG

  g_object_unref (conf);
  g_object_unref (builder);

  return dlg;
}

static UniqueResponse
message_received (UniqueApp * app, gint command, UniqueMessageData *message_data,
                  guint time, gpointer user_data)
{
  if (command == UNIQUE_ACTIVATE) {
    gtk_window_present_with_time (GTK_WINDOW (user_data), time);
    return UNIQUE_RESPONSE_OK;
  }
  return UNIQUE_RESPONSE_PASSTHROUGH;
}

int
main (int argc, char ** argv)
{
	g_type_init ();

	/* Setting up i18n and gettext.  Apparently, we need
	   all of these. */
	setlocale (LC_ALL, "");
	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	textdomain (GETTEXT_PACKAGE);

	gtk_init (&argc, &argv);

	UniqueApp * app = unique_app_new ("com.canonical.indicator.datetime.preferences", NULL);

  if (unique_app_is_running (app)) {
    unique_app_send_message (app, UNIQUE_ACTIVATE, NULL);
  } else {
    // We're first instance.  Yay!
    GtkWidget * dlg = create_dialog ();

    g_signal_connect (app, "message-received", G_CALLBACK(message_received), dlg);
    unique_app_watch_window (app, GTK_WINDOW (dlg));

    gtk_widget_show_all (dlg);
    g_signal_connect (dlg, "response", G_CALLBACK(gtk_main_quit), NULL);
    gtk_main ();
  }

  return 0;
}

