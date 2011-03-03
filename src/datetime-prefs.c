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

#include <stdlib.h>
#include <libintl.h>
#include <locale.h>
#include <langinfo.h>
#include <glib/gi18n-lib.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <unique/unique.h>
#include <polkitgtk/polkitgtk.h>

#include "settings-shared.h"
#include "utils.h"
#include "datetime-prefs-locations.h"
#include "timezone-completion.h"
#include "cc-timezone-map.h"

#define DATETIME_DIALOG_UI_FILE PKGDATADIR "/datetime-dialog.ui"

GDBusProxy * proxy = NULL;
GtkWidget * auto_radio = NULL;
GtkWidget * tz_entry = NULL;
CcTimezoneMap * tzmap = NULL;
GtkWidget * time_spin = NULL;
GtkWidget * date_spin = NULL;
guint       save_time_id = 0;
gboolean    user_edited_time = FALSE;
gboolean    changing_time = FALSE;

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

static void
polkit_dependency_cb (GtkWidget * parent, GParamSpec *pspec, GtkWidget * dependent)
{
  gboolean authorized, sensitive;
  g_object_get (G_OBJECT (parent),
                "is-authorized", &authorized,
                "sensitive", &sensitive, NULL);
  gtk_widget_set_sensitive (dependent, authorized && sensitive);
}

static void
add_polkit_dependency (GtkWidget * parent, GtkWidget * dependent)
{
  g_signal_connect (parent, "notify::is-authorized", G_CALLBACK(polkit_dependency_cb),
                    dependent);
  g_signal_connect (parent, "notify::sensitive", G_CALLBACK(polkit_dependency_cb),
                    dependent);
  polkit_dependency_cb (parent, NULL, dependent);
}

static void
dbus_set_answered (GObject *object, GAsyncResult *res, gpointer command)
{
  GError * error = NULL;
  GVariant * answers = g_dbus_proxy_call_finish (proxy, res, &error);

  if (error != NULL) {
    g_warning("Could not set '%s' for SettingsDaemon: %s", (gchar *)command, error->message);
    g_error_free(error);
    return;
  }

  g_variant_unref (answers);
}

static void
toggle_ntp (GtkWidget * radio, GParamSpec * pspec, gpointer user_data)
{
  gboolean active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (radio));

  g_dbus_proxy_call (proxy, "SetUsingNtp", g_variant_new ("(b)", active),
                     G_DBUS_CALL_FLAGS_NONE, -1, NULL, dbus_set_answered, "using_ntp");
}

static void
ntp_query_answered (GObject *object, GAsyncResult *res, gpointer user_data)
{
  GError * error = NULL;
  GVariant * answers = g_dbus_proxy_call_finish (proxy, res, &error);

  if (error != NULL) {
    g_warning("Could not query DBus proxy for SettingsDaemon: %s", error->message);
    g_error_free(error);
    return;
  }

  gboolean can_use_ntp, is_using_ntp;
  g_variant_get (answers, "(bb)", &can_use_ntp, &is_using_ntp);

  gtk_widget_set_sensitive (GTK_WIDGET (auto_radio), can_use_ntp);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (auto_radio), is_using_ntp);

  g_signal_connect (auto_radio, "notify::active", G_CALLBACK (toggle_ntp), NULL);

  g_variant_unref (answers);
}

static void
sync_entry (const gchar * location)
{
  gchar * name;
  split_settings_location (location, NULL, &name);
  gtk_entry_set_text (GTK_ENTRY (tz_entry), name);
  g_free (name);
}

static void
tz_changed (CcTimezoneMap * map, TzLocation * location)
{
  if (location == NULL)
    return;

  gchar * file = g_build_filename ("/usr/share/zoneinfo", location->zone, NULL);
  g_dbus_proxy_call (proxy, "SetTimezone", g_variant_new ("(s)", file),
                     G_DBUS_CALL_FLAGS_NONE, -1, NULL, dbus_set_answered, "timezone");
  g_free (file);

  sync_entry (location->zone);
}

static void
tz_query_answered (GObject *object, GAsyncResult *res, gpointer user_data)
{
  GError * error = NULL;
  GVariant * answers = g_dbus_proxy_call_finish (proxy, res, &error);

  if (error != NULL) {
    g_warning("Could not query DBus proxy for SettingsDaemon: %s", error->message);
    g_error_free(error);
    return;
  }

  const gchar * timezone;
  g_variant_get (answers, "(&s)", &timezone);

  cc_timezone_map_set_timezone (tzmap, timezone);

  sync_entry (timezone);
  g_signal_connect (tzmap, "location-changed", G_CALLBACK (tz_changed), NULL);

  g_variant_unref (answers);
}

void proxy_ready (GObject *object, GAsyncResult *res, gpointer user_data)
{
  GError * error = NULL;

  proxy = g_dbus_proxy_new_for_bus_finish (res, &error);

  if (error != NULL) {
    g_critical("Could not grab DBus proxy for SettingsDaemon: %s", error->message);
    g_error_free(error);
    return;
  }

  /* And now, do initial proxy configuration */
  g_dbus_proxy_call (proxy, "GetUsingNtp", NULL, G_DBUS_CALL_FLAGS_NONE, -1,
                     NULL, ntp_query_answered, auto_radio);
  g_dbus_proxy_call (proxy, "GetTimezone", NULL, G_DBUS_CALL_FLAGS_NONE, -1,
                     NULL, tz_query_answered, NULL);
}

static gboolean
are_spinners_focused (void)
{
  // save_time_id means that we were in focus and haven't finished our save
  // yet, so act like we are still focused.
  return save_time_id || gtk_widget_has_focus (time_spin) || gtk_widget_has_focus (date_spin);
}

static gboolean
save_time (gpointer user_data)
{
  if (user_edited_time) {
    gdouble current_value = gtk_spin_button_get_value (GTK_SPIN_BUTTON (date_spin));
    g_dbus_proxy_call (proxy, "SetTime", g_variant_new ("(x)", (guint64)current_value),
                       G_DBUS_CALL_FLAGS_NONE, -1, NULL, dbus_set_answered, "time");
  }
  user_edited_time = FALSE;
  save_time_id = 0;
  return FALSE;
}

static gboolean
spin_focus_in (void)
{
  if (save_time_id > 0) {
    g_source_remove (save_time_id);
    save_time_id = 0;
  }
  return FALSE;
}

static gboolean
spin_focus_out (void)
{
  /* We want to only save when both spinners are unfocused.  But it's difficult
     to tell who is about to get focus during a focus-out.  So we set an idle
     callback to save the time if we don't focus in to another spinner by that
     time. */
  if (save_time_id == 0) {
    save_time_id = g_idle_add ((GSourceFunc)save_time, NULL);
  }
  return FALSE;
}

static int
input_time_text (GtkWidget * spinner, gdouble * value, gpointer user_data)
{
  gboolean is_time = (gboolean)GPOINTER_TO_INT (g_object_get_data (G_OBJECT (spinner), "is-time"));
  const gchar * text = gtk_entry_get_text (GTK_ENTRY (spinner));

  gdouble current_value = gtk_spin_button_get_value (GTK_SPIN_BUTTON (spinner));
  *value = current_value;

  GDateTime * now = g_date_time_new_from_unix_local (current_value);
  gint year, month, day, hour, minute, second;
  year = g_date_time_get_year (now);
  month = g_date_time_get_month (now);
  day = g_date_time_get_day_of_month (now);
  hour = g_date_time_get_hour (now);
  minute = g_date_time_get_minute (now);
  second = g_date_time_get_second (now);
  g_date_time_unref (now);

  /* Parse this string as if it were in the output format */
  gint scanned = 0;
  gboolean passed = TRUE, skip = FALSE;
  if (is_time) {
    gint hour_in, minute_in, second_in;

    if (is_locale_12h ()) { // TODO: make this look-at/watch gsettings?
      char ampm[51];

      scanned = sscanf (text, "%u:%u:%u %50s", &hour_in, &minute_in, &second_in, ampm);
      passed = (scanned == 4);

      if (passed) {
        const char *pm_str = nl_langinfo (PM_STR);
        if (g_ascii_strcasecmp (pm_str, ampm) == 0) {
          hour_in += 12;
        }
      }
    } else {
      scanned = sscanf (text, "%u:%u:%u", &hour_in, &minute_in, &second_in);
      passed = (scanned == 3);
    }

    if (passed && (hour_in > 23 || minute_in > 59 || second_in > 59)) {
      passed = FALSE;
    }
    if (passed && hour == hour_in && minute == minute_in && second == second_in) {
      skip = TRUE; // no change
    } else {
      hour = hour_in;
      minute = minute_in;
      second = second_in;
    }
  }
  else {
    gint year_in, month_in, day_in;

    scanned = sscanf (text, "%u-%u-%u", &year_in, &month_in, &day_in);

    if (scanned != 3 || year_in < 1 || year_in > 9999 ||
        month_in < 1 || month_in > 12 || day_in < 1 || day_in > 31) {
      passed = FALSE;
    }
    if (passed && year == year_in && month == month_in && day == day_in) {
      skip = TRUE; // no change
    } else {
      year = year_in;
      month = month_in;
      day = day_in;
    }
  }

  if (!passed) {
    g_warning ("Could not understand %s", text);
    return TRUE;
  }

  if (skip) {
    return TRUE;
  }

  gboolean prev_changing = changing_time;
  changing_time = TRUE;
  GDateTime * new_time = g_date_time_new_local (year, month, day, hour, minute, second);
  *value = g_date_time_to_unix (new_time);
  user_edited_time = TRUE;
  g_date_time_unref (new_time);
  changing_time = prev_changing;

  return TRUE;
}

static gboolean
format_time_text (GtkWidget * spinner, gpointer user_data)
{
  gboolean is_time = (gboolean)GPOINTER_TO_INT (g_object_get_data (G_OBJECT (spinner), "is-time"));

  const gchar * format;
  if (is_time) {
    if (is_locale_12h ()) { // TODO: make this look-at/watch gsettings?
      format = "%I:%M:%S %p";
    } else {
      format = "%H:%M:%S";
    }
  }
  else {
    format = "%Y-%m-%d";
  }

  GDateTime * datetime = g_date_time_new_from_unix_local (gtk_spin_button_get_value (GTK_SPIN_BUTTON (spinner)));
  gchar * formatted = g_date_time_format (datetime, format);
  gtk_entry_set_text (GTK_ENTRY (spinner), formatted);
  g_date_time_unref (datetime);

  return TRUE;
}

static void
spin_copy_value (GtkSpinButton * spinner, GtkSpinButton * other)
{
  if (gtk_spin_button_get_value (spinner) != gtk_spin_button_get_value (other)) {
    gtk_spin_button_set_value (other, gtk_spin_button_get_value (spinner));
  }
  if (!changing_time) { /* Means user pressed spin buttons */
    user_edited_time = TRUE;
  }
}

static gboolean
update_spinners (void)
{
  /* Add datetime object to spinner, which will hold the real time value, rather
     then using the value of the spinner itself.  And don't update while user is
     editing. */
  if (!are_spinners_focused ()) {
    gboolean prev_changing = changing_time;
    changing_time = TRUE;
    GDateTime * now = g_date_time_new_now_local ();
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (time_spin), (gdouble)g_date_time_to_unix (now));
    /* will be copied to other spin button */
    g_date_time_unref (now);
    changing_time = prev_changing;
  }
  return TRUE;
}

static void
setup_time_spinners (GtkWidget * time, GtkWidget * date)
{
  g_signal_connect (time, "input", G_CALLBACK (input_time_text), date);
  g_signal_connect (date, "input", G_CALLBACK (input_time_text), time);

  g_signal_connect (time, "output", G_CALLBACK (format_time_text), date);
  g_signal_connect (date, "output", G_CALLBACK (format_time_text), time);

  g_signal_connect (time, "focus-in-event", G_CALLBACK (spin_focus_in), date);
  g_signal_connect (date, "focus-in-event", G_CALLBACK (spin_focus_in), time);

  g_signal_connect (time, "focus-out-event", G_CALLBACK (spin_focus_out), date);
  g_signal_connect (date, "focus-out-event", G_CALLBACK (spin_focus_out), time);

  g_signal_connect (time, "value-changed", G_CALLBACK (spin_copy_value), date);
  g_signal_connect (date, "value-changed", G_CALLBACK (spin_copy_value), time);

  g_object_set_data (G_OBJECT (time), "is-time", GINT_TO_POINTER (TRUE));
  g_object_set_data (G_OBJECT (date), "is-time", GINT_TO_POINTER (FALSE));

  time_spin = time;
  date_spin = date;

  /* 2 seconds is what the indicator itself uses */
  guint time_id = g_timeout_add_seconds (2, (GSourceFunc)update_spinners, NULL);
  g_signal_connect_swapped (time_spin, "destroy", G_CALLBACK (g_source_remove), GINT_TO_POINTER (time_id));
  update_spinners ();
}

static void
show_locations (GtkWidget * button, GtkWidget * dlg)
{
  GtkWidget * locationsDlg = datetime_setup_locations_dialog (GTK_WINDOW (dlg), tzmap);
  gtk_widget_show_all (locationsDlg);
}

static gboolean
timezone_selected (GtkEntryCompletion * widget, GtkTreeModel * model,
                   GtkTreeIter * iter, gpointer user_data)
{
  GValue value = {0};
  const gchar * strval;

  gtk_tree_model_get_value (model, iter, TIMEZONE_COMPLETION_ZONE, &value);
  strval = g_value_get_string (&value);

  if (strval != NULL && strval[0] != 0) {
    cc_timezone_map_set_timezone (tzmap, strval);
  }
  else {
    GValue lon_value = {0}, lat_value = {0};
    const gchar * strlon, * strlat;
    gdouble lon = 0.0, lat = 0.0;

    gtk_tree_model_get_value (model, iter, TIMEZONE_COMPLETION_LONGITUDE, &lon_value);
    strlon = g_value_get_string (&lon_value);
    if (strlon != NULL && strlon[0] != 0) {
      lon = strtod(strlon, NULL);
    }

    gtk_tree_model_get_value (model, iter, TIMEZONE_COMPLETION_LATITUDE, &lat_value);
    strlat = g_value_get_string (&lat_value);
    if (strlat != NULL && strlat[0] != 0) {
      lat = strtod(strlat, NULL);
    }

    cc_timezone_map_set_coords (tzmap, lon, lat);
  }

  g_value_unset (&value);

  return FALSE; // Do normal action too
}

static gboolean
key_pressed (GtkWidget * widget, GdkEventKey * event, gpointer user_data)
{
  switch (event->keyval) {
  case GDK_KEY_Escape:
    gtk_widget_destroy (widget);
    return TRUE;
  }
  return FALSE;
}

static GtkWidget *
get_child_of_type (GtkContainer * parent, GType type)
{
  GList * children, * iter;

  children = gtk_container_get_children (parent);
  for (iter = children; iter; iter = iter->next) {
    if (G_TYPE_CHECK_INSTANCE_TYPE (iter->data, type)) {
      return GTK_WIDGET (iter->data);
    }
  }

  return NULL;
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

  /* Add policykit button */
  GtkWidget * polkit_button = polkit_lock_button_new ("org.gnome.settingsdaemon.datetimemechanism.configure");
  polkit_lock_button_set_unlock_text (POLKIT_LOCK_BUTTON (polkit_button), _("Unlock to change these settings"));
  polkit_lock_button_set_lock_text (POLKIT_LOCK_BUTTON (polkit_button), _("Lock to prevent further changes"));
  gtk_box_pack_start (GTK_BOX (WIG ("timeDateBox")), polkit_button, FALSE, TRUE, 0);
  /* Make sure border around button is visible */
  GtkWidget * polkit_button_button = get_child_of_type (GTK_CONTAINER (polkit_button), GTK_TYPE_BUTTON);
  if (polkit_button_button != NULL) {
    gtk_button_set_relief (GTK_BUTTON (polkit_button_button), GTK_RELIEF_NORMAL);
  }

  /* Add map */
  tzmap = cc_timezone_map_new ();
  gtk_container_add (GTK_CONTAINER (WIG ("mapBox")), GTK_WIDGET (tzmap));
  /* Fufill the CC by Attribution license requirements for the Geonames lookup */
  cc_timezone_map_set_watermark (tzmap, "Geonames.org");

  /* And completion entry */
  TimezoneCompletion * completion = timezone_completion_new ();
  gtk_entry_set_completion (GTK_ENTRY (WIG ("timezoneEntry")),
                            GTK_ENTRY_COMPLETION (completion));
  timezone_completion_watch_entry (completion, GTK_ENTRY (WIG ("timezoneEntry")));
  g_signal_connect (completion, "match-selected", G_CALLBACK (timezone_selected), NULL);

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
  g_settings_bind (conf, SETTINGS_SHOW_EVENTS_S, WIG ("showEventsCheck"),
                   "active", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (conf, SETTINGS_SHOW_LOCATIONS_S, WIG ("showLocationsCheck"),
                   "active", G_SETTINGS_BIND_DEFAULT);

  /* Set up sensitivities */
  add_widget_dependency (WIG ("showCalendarCheck"), WIG ("calendarOptions"));
  add_widget_dependency (WIG ("showClockCheck"), WIG ("clockOptions"));
  add_widget_dependency (WIG ("showLocationsCheck"), WIG ("locationsButton"));
  add_widget_dependency (WIG ("manualTimeRadio"), WIG ("manualOptions"));
  add_polkit_dependency (polkit_button, WIG ("timeDateOptions"));

  /* Hacky proxy test for whether evolution-data-server is installed */
  gchar * evo_path = g_find_program_in_path ("evolution");
  gtk_widget_set_sensitive (WIG ("showEventsCheck"), (evo_path != NULL));
  g_free (evo_path);

  setup_time_spinners (WIG ("timeSpinner"), WIG ("dateSpinner"));

  GtkWidget * dlg = WIG ("timeDateDialog");
  auto_radio = WIG ("automaticTimeRadio");
  tz_entry = WIG ("timezoneEntry");

  g_signal_connect (WIG ("locationsButton"), "clicked", G_CALLBACK (show_locations), dlg);
  g_signal_connect (dlg, "key-press-event", G_CALLBACK (key_pressed), NULL);

  /* Grab proxy for settings daemon */
  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE, NULL,
                            "org.gnome.SettingsDaemon.DateTimeMechanism",
                            "/",                            
                            "org.gnome.SettingsDaemon.DateTimeMechanism",
                            NULL, proxy_ready, NULL);

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
    g_signal_connect (dlg, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    gtk_main ();
  }

  return 0;
}

