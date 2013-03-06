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
#include <polkit/polkit.h>
#include <libgnome-control-center/cc-panel.h>
#include <timezonemap/cc-timezone-map.h>
#include <timezonemap/timezone-completion.h>

#include "dbus-shared.h"
#include "settings-shared.h"
#include "utils.h"
#include "datetime-prefs-locations.h"

#define DATETIME_DIALOG_UI_FILE PKGDATADIR "/datetime-dialog.ui"

#define INDICATOR_DATETIME_TYPE_PANEL indicator_datetime_panel_get_type()

typedef struct _IndicatorDatetimePanel IndicatorDatetimePanel;
typedef struct _IndicatorDatetimePanelPrivate IndicatorDatetimePanelPrivate;
typedef struct _IndicatorDatetimePanelClass IndicatorDatetimePanelClass;

struct _IndicatorDatetimePanel
{
  CcPanel parent;
  IndicatorDatetimePanelPrivate * priv;
};

struct _IndicatorDatetimePanelPrivate
{
  GtkBuilder *         builder;
  GDBusProxy *         proxy;
  GtkWidget *          auto_radio;
  GtkWidget *          tz_entry;
  CcTimezoneMap *      tzmap;
  GtkWidget *          time_spin;
  GtkWidget *          date_spin;
  guint                save_time_id;
  gboolean             user_edited_time;
  gboolean             changing_time;
  GtkWidget *          loc_dlg;
  CcTimezoneCompletion * completion;
  GCancellable         * tz_query_cancel;
  GCancellable         * ntp_query_cancel;
};

struct _IndicatorDatetimePanelClass
{
  CcPanelClass parent_class;
};

G_DEFINE_DYNAMIC_TYPE (IndicatorDatetimePanel, indicator_datetime_panel, CC_TYPE_PANEL)

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
polkit_dependency_cb (GPermission * permission, GParamSpec *pspec, GtkWidget * dependent)
{
  gboolean allowed = FALSE;

  g_object_get (G_OBJECT (permission),
                "allowed", &allowed, NULL);

  gtk_widget_set_sensitive (dependent, allowed);
}

static void
add_polkit_dependency_helper (GtkWidget * parent, GParamSpec *pspec, GtkWidget * dependent)
{
  GtkLockButton * button = GTK_LOCK_BUTTON (parent);
  GPermission * permission = gtk_lock_button_get_permission (button);
  g_signal_connect (permission, "notify::allowed",
                    G_CALLBACK(polkit_dependency_cb), dependent);
  polkit_dependency_cb (permission, NULL, dependent);
}

static void
add_polkit_dependency (GtkWidget * parent, GtkWidget * dependent)
{
  /* polkit async hasn't finished at this point, so wait for permission to come in */
  g_signal_connect (parent, "notify::permission", G_CALLBACK(add_polkit_dependency_helper),
                    dependent);
  gtk_widget_set_sensitive (dependent, FALSE);
}

static void
polkit_perm_ready (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  GError * error = NULL;
  GPermission * permission = polkit_permission_new_finish (res, &error);

  if (error != NULL) {
    g_warning ("Could not get permission object: %s", error->message);
    g_error_free (error);
    return;
  }

  GtkLockButton * button = GTK_LOCK_BUTTON (user_data);
  gtk_lock_button_set_permission (button, permission);
}

static void
dbus_set_answered (GObject *object, GAsyncResult *res, gpointer command)
{
  GError * error = NULL;
  GVariant * answers = g_dbus_proxy_call_finish (G_DBUS_PROXY (object), res, &error);

  if (error != NULL) {
    g_warning("Could not set '%s' for SettingsDaemon: %s", (gchar *)command, error->message);
    g_error_free(error);
    return;
  }

  g_variant_unref (answers);
}

static void
toggle_ntp (GtkWidget * radio, GParamSpec * pspec, IndicatorDatetimePanel * self)
{
  gboolean active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (radio));

  g_dbus_proxy_call (self->priv->proxy, "SetUsingNtp", g_variant_new ("(b)", active),
                     G_DBUS_CALL_FLAGS_NONE, -1, NULL, dbus_set_answered, "using_ntp");
}

static void
ntp_query_answered (GObject *object, GAsyncResult *res, IndicatorDatetimePanel * self)
{
  GError * error = NULL;
  GVariant * answers = g_dbus_proxy_call_finish (G_DBUS_PROXY (object), res, &error);

  g_clear_object (&self->priv->ntp_query_cancel);

  if (error != NULL) {
    g_warning("Could not query DBus proxy for SettingsDaemon: %s", error->message);
    g_error_free(error);
    return;
  }

  gboolean can_use_ntp, is_using_ntp;
  g_variant_get (answers, "(bb)", &can_use_ntp, &is_using_ntp);

  gtk_widget_set_sensitive (GTK_WIDGET (self->priv->auto_radio), can_use_ntp);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->priv->auto_radio), is_using_ntp);

  g_signal_connect (self->priv->auto_radio, "notify::active", G_CALLBACK (toggle_ntp), self);

  g_variant_unref (answers);
}

static void
sync_entry (IndicatorDatetimePanel * self, const gchar * location)
{
  gchar * name = get_current_zone_name (location);
  gtk_entry_set_text (GTK_ENTRY (self->priv->tz_entry), name);
  g_free (name);

  gtk_entry_set_icon_from_stock (GTK_ENTRY (self->priv->tz_entry),
                                 GTK_ENTRY_ICON_SECONDARY, NULL);
}

static void
tz_changed (CcTimezoneMap * map, CcTimezoneLocation * location, IndicatorDatetimePanel * self)
{
  if (location == NULL)
    return;

  gchar * zone;
  g_object_get (location, "zone", &zone, NULL);

  g_dbus_proxy_call (self->priv->proxy, "SetTimezone", g_variant_new ("(s)", zone),
                     G_DBUS_CALL_FLAGS_NONE, -1, NULL, dbus_set_answered, "timezone");

  sync_entry (self, zone);

  g_free (zone);
}

static void
tz_query_answered (GObject *object, GAsyncResult *res, IndicatorDatetimePanel * self)
{
  GError * error = NULL;
  GVariant * answers = g_dbus_proxy_call_finish (G_DBUS_PROXY (object), res, &error);

  g_clear_object (&self->priv->tz_query_cancel);

  if (error != NULL) {
    g_warning("Could not query DBus proxy for SettingsDaemon: %s", error->message);
    g_error_free(error);
    return;
  }

  const gchar * timezone;
  g_variant_get (answers, "(&s)", &timezone);

  cc_timezone_map_set_timezone (self->priv->tzmap, timezone);

  sync_entry (self, timezone);
  g_signal_connect (self->priv->tzmap, "location-changed", G_CALLBACK (tz_changed), self);

  g_variant_unref (answers);
}

static void
proxy_ready (GObject *object, GAsyncResult *res, IndicatorDatetimePanel * self)
{
  GError * error = NULL;
  IndicatorDatetimePanelPrivate * priv = self->priv;

  self->priv->proxy = g_dbus_proxy_new_for_bus_finish (res, &error);

  if (error != NULL) {
    g_critical("Could not grab DBus proxy for SettingsDaemon: %s", error->message);
    g_error_free(error);
    return;
  }

  /* And now, do initial proxy configuration */
  if (priv->ntp_query_cancel == NULL) {
    priv->ntp_query_cancel = g_cancellable_new();
    g_dbus_proxy_call (priv->proxy, "GetUsingNtp", NULL, G_DBUS_CALL_FLAGS_NONE, -1,
                       priv->ntp_query_cancel, (GAsyncReadyCallback)ntp_query_answered, self);
  }
  if (priv->tz_query_cancel == NULL) {
    priv->tz_query_cancel = g_cancellable_new();
    g_dbus_proxy_call (priv->proxy, "GetTimezone", NULL, G_DBUS_CALL_FLAGS_NONE, -1,
                       priv->tz_query_cancel, (GAsyncReadyCallback)tz_query_answered, self);
  }
}

static void
service_name_owner_changed (GDBusProxy * proxy, GParamSpec *pspec, gpointer user_data)
{
  GtkWidget * widget = GTK_WIDGET (user_data);
  gchar * owner = g_dbus_proxy_get_name_owner (proxy);

  gtk_widget_set_sensitive (widget, (owner != NULL));

  g_free (owner);
}

static void
service_proxy_ready (GObject *object, GAsyncResult *res, gpointer user_data)
{
  GError * error = NULL;

  GDBusProxy * proxy = g_dbus_proxy_new_for_bus_finish (res, &error);

  if (error != NULL) {
    g_critical("Could not grab DBus proxy for indicator-datetime-service: %s", error->message);
    g_error_free(error);
    return;
  }

  /* And now, do initial proxy configuration */
  g_signal_connect (proxy, "notify::g-name-owner", G_CALLBACK (service_name_owner_changed), user_data);
  service_name_owner_changed (proxy, NULL, user_data);
}

static gboolean
are_spinners_focused (IndicatorDatetimePanel * self)
{
  // save_time_id means that we were in focus and haven't finished our save
  // yet, so act like we are still focused.
  return self->priv->save_time_id ||
         gtk_widget_has_focus (self->priv->time_spin) ||
         gtk_widget_has_focus (self->priv->date_spin);
}

static gboolean
save_time (IndicatorDatetimePanel * self)
{
  if (self->priv->user_edited_time) {
    gdouble current_value = gtk_spin_button_get_value (GTK_SPIN_BUTTON (self->priv->date_spin));
    g_dbus_proxy_call (self->priv->proxy, "SetTime", g_variant_new ("(x)", (guint64)current_value),
                       G_DBUS_CALL_FLAGS_NONE, -1, NULL, dbus_set_answered, "time");
  }
  self->priv->user_edited_time = FALSE;
  self->priv->save_time_id = 0;
  return FALSE;
}

static gboolean
spin_focus_in (IndicatorDatetimePanel * self)
{
  if (self->priv->save_time_id > 0) {
    g_source_remove (self->priv->save_time_id);
    self->priv->save_time_id = 0;
  }
  return FALSE;
}

static gboolean
spin_focus_out (IndicatorDatetimePanel * self)
{
  /* We want to only save when both spinners are unfocused.  But it's difficult
     to tell who is about to get focus during a focus-out.  So we set an idle
     callback to save the time if we don't focus in to another spinner by that
     time. */
  if (self->priv->save_time_id == 0) {
    self->priv->save_time_id = g_idle_add ((GSourceFunc)save_time, self);
  }
  return FALSE;
}

static int
input_time_text (GtkWidget * spinner, gdouble * value, IndicatorDatetimePanel * self)
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

      /* coverity[secure_coding] */
      scanned = sscanf (text, "%u:%u:%u %50s", &hour_in, &minute_in, &second_in, ampm);
      passed = (scanned == 4);

      if (passed) {
        const char *pm_str = nl_langinfo (PM_STR);
        if (g_ascii_strcasecmp (pm_str, ampm) == 0) {
          hour_in += 12;
        }
      }
    } else {
      /* coverity[secure_coding] */
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

    /* coverity[secure_coding] */
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

  gboolean prev_changing = self->priv->changing_time;
  self->priv->changing_time = TRUE;
  GDateTime * new_time = g_date_time_new_local (year, month, day, hour, minute, second);
  *value = g_date_time_to_unix (new_time);
  self->priv->user_edited_time = TRUE;
  g_date_time_unref (new_time);
  self->priv->changing_time = prev_changing;

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
    // This is intentionally not "%x".  See https://launchpad.net/bugs/1149696
    // If you are willing to do the hard work of writing a locale-sensitive
    // date parser, there is an open bug: https://launchpad.net/bugs/729056
    format = "%Y-%m-%d";
  }

  GDateTime * datetime = g_date_time_new_from_unix_local (gtk_spin_button_get_value (GTK_SPIN_BUTTON (spinner)));
  gchar * formatted = g_date_time_format (datetime, format);
  gtk_entry_set_text (GTK_ENTRY (spinner), formatted);
  g_date_time_unref (datetime);

  return TRUE;
}

static void
spin_copy_value (GtkSpinButton * spinner, IndicatorDatetimePanel * self)
{
  GtkSpinButton * other = NULL;
  if (GTK_WIDGET (spinner) == self->priv->date_spin)
    other = GTK_SPIN_BUTTON (self->priv->time_spin);
  else
    other = GTK_SPIN_BUTTON (self->priv->date_spin);

  if (gtk_spin_button_get_value (spinner) != gtk_spin_button_get_value (other)) {
    gtk_spin_button_set_value (other, gtk_spin_button_get_value (spinner));
  }
  if (!self->priv->changing_time) { /* Means user pressed spin buttons */
    self->priv->user_edited_time = TRUE;
  }
}

static gboolean
update_spinners (IndicatorDatetimePanel * self)
{
  /* Add datetime object to spinner, which will hold the real time value, rather
     then using the value of the spinner itself.  And don't update while user is
     editing. */
  if (!are_spinners_focused (self)) {
    gboolean prev_changing = self->priv->changing_time;
    self->priv->changing_time = TRUE;
    GDateTime * now = g_date_time_new_now_local ();
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (self->priv->time_spin),
                               (gdouble)g_date_time_to_unix (now));
    /* will be copied to other spin button */
    g_date_time_unref (now);
    self->priv->changing_time = prev_changing;
  }
  return TRUE;
}

static void
setup_time_spinners (IndicatorDatetimePanel * self, GtkWidget * time, GtkWidget * date)
{
  g_signal_connect (time, "input", G_CALLBACK (input_time_text), self);
  g_signal_connect (date, "input", G_CALLBACK (input_time_text), self);

  g_signal_connect (time, "output", G_CALLBACK (format_time_text), date);
  g_signal_connect (date, "output", G_CALLBACK (format_time_text), time);

  g_signal_connect_swapped (time, "focus-in-event", G_CALLBACK (spin_focus_in), self);
  g_signal_connect_swapped (date, "focus-in-event", G_CALLBACK (spin_focus_in), self);

  g_signal_connect_swapped (time, "focus-out-event", G_CALLBACK (spin_focus_out), self);
  g_signal_connect_swapped (date, "focus-out-event", G_CALLBACK (spin_focus_out), self);

  g_signal_connect (time, "value-changed", G_CALLBACK (spin_copy_value), self);
  g_signal_connect (date, "value-changed", G_CALLBACK (spin_copy_value), self);

  g_object_set_data (G_OBJECT (time), "is-time", GINT_TO_POINTER (TRUE));
  g_object_set_data (G_OBJECT (date), "is-time", GINT_TO_POINTER (FALSE));

  self->priv->time_spin = time;
  self->priv->date_spin = date;

  /* 2 seconds is what the indicator itself uses */
  guint time_id = g_timeout_add_seconds (2, (GSourceFunc)update_spinners, self);
  g_signal_connect_swapped (self->priv->time_spin, "destroy",
                            G_CALLBACK (g_source_remove), GINT_TO_POINTER (time_id));
  update_spinners (self);
}

static void
show_locations (IndicatorDatetimePanel * self)
{
  if (self->priv->loc_dlg == NULL) {
    self->priv->loc_dlg = datetime_setup_locations_dialog (self->priv->tzmap);
    GtkWidget * dlg = gtk_widget_get_toplevel (GTK_WIDGET (self));
    gtk_window_set_type_hint (GTK_WINDOW(self->priv->loc_dlg), GDK_WINDOW_TYPE_HINT_DIALOG);
    gtk_window_set_transient_for (GTK_WINDOW (self->priv->loc_dlg), GTK_WINDOW (dlg));
    g_signal_connect (self->priv->loc_dlg, "destroy", G_CALLBACK (gtk_widget_destroyed), &self->priv->loc_dlg);
    gtk_widget_show_all (self->priv->loc_dlg);
  }
  else {
    gtk_window_present_with_time (GTK_WINDOW (self->priv->loc_dlg), gtk_get_current_event_time ());
  }
}

static gboolean
timezone_selected (GtkEntryCompletion * widget, GtkTreeModel * model,
                   GtkTreeIter * iter, IndicatorDatetimePanel * self)
{
  const gchar * name, * zone;

  gtk_tree_model_get (model, iter,
                      CC_TIMEZONE_COMPLETION_NAME, &name,
                      CC_TIMEZONE_COMPLETION_ZONE, &zone,
                      -1);

  if (zone == NULL || zone[0] == 0) {
    const gchar * strlon, * strlat;
    gdouble lon = 0.0, lat = 0.0;

    gtk_tree_model_get (model, iter,
                        CC_TIMEZONE_COMPLETION_LONGITUDE, &strlon,
                        CC_TIMEZONE_COMPLETION_LATITUDE, &strlat,
                        -1);

    if (strlon != NULL && strlon[0] != 0) {
      lon = g_ascii_strtod(strlon, NULL);
    }

    if (strlat != NULL && strlat[0] != 0) {
      lat = g_ascii_strtod(strlat, NULL);
    }

    zone = cc_timezone_map_get_timezone_at_coords (self->priv->tzmap, lon, lat);
  }

  GSettings * conf = g_settings_new (SETTINGS_INTERFACE);
  gchar * tz_name = g_strdup_printf ("%s %s", zone, name);
  g_settings_set_string (conf, SETTINGS_TIMEZONE_NAME_S, tz_name);
  g_free (tz_name);
  g_object_unref (conf);

  cc_timezone_map_set_timezone (self->priv->tzmap, zone);

  return FALSE; // Do normal action too
}

static gboolean
entry_focus_out (GtkEntry * entry, GdkEventFocus * event, IndicatorDatetimePanel * self)
{
  // If the name left in the entry doesn't match the current timezone name,
  // show an error icon.  It's always an error for the user to manually type in
  // a timezone.
  CcTimezoneLocation * location = cc_timezone_map_get_location (self->priv->tzmap);
  if (location == NULL)
    return FALSE;

  gchar * zone;
  g_object_get (location, "zone", &zone, NULL);

  gchar * name = get_current_zone_name (zone);
  gboolean correct = (g_strcmp0 (gtk_entry_get_text (entry), name) == 0);
  g_free (name);
  g_free (zone);

  gtk_entry_set_icon_from_stock (entry, GTK_ENTRY_ICON_SECONDARY,
                                 correct ? NULL : GTK_STOCK_DIALOG_ERROR);
  gtk_entry_set_icon_tooltip_text (entry, GTK_ENTRY_ICON_SECONDARY,
                                   _("You need to choose a location to change the time zone."));
  gtk_entry_set_icon_activatable (entry, GTK_ENTRY_ICON_SECONDARY, FALSE);
  return FALSE;
}

static void
indicator_datetime_panel_init (IndicatorDatetimePanel * self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            INDICATOR_DATETIME_TYPE_PANEL,
                                            IndicatorDatetimePanelPrivate);

  GError * error = NULL;

  self->priv->builder = gtk_builder_new ();
  gtk_builder_set_translation_domain (self->priv->builder, GETTEXT_PACKAGE);
  gtk_builder_add_from_file (self->priv->builder, DATETIME_DIALOG_UI_FILE, &error);
  if (error != NULL) {
    /* We have to abort, we can't continue without the ui file */
    g_error ("Could not load ui file %s: %s", DATETIME_DIALOG_UI_FILE, error->message);
    g_error_free (error);
    return;
  }

  GSettings * conf = g_settings_new (SETTINGS_INTERFACE);

#define WIG(name) GTK_WIDGET (gtk_builder_get_object (self->priv->builder, name))

  /* Add policykit button */
  GtkWidget * polkit_button = gtk_lock_button_new (NULL);
  g_object_set (G_OBJECT (polkit_button),
                "text-unlock", _("Unlock to change these settings"),
                "text-lock", _("Lock to prevent further changes"),
                NULL);
  GtkWidget * alignment = gtk_alignment_new (0.0, 0.5, 0.0, 0.0);
  gtk_container_add (GTK_CONTAINER (alignment), polkit_button);
  gtk_box_pack_start (GTK_BOX (WIG ("timeDateBox")), alignment, FALSE, TRUE, 0);

  const gchar * polkit_name = "org.gnome.settingsdaemon.datetimemechanism.configure";
  polkit_permission_new (polkit_name, NULL, NULL, polkit_perm_ready, polkit_button);

  /* Add map */
  self->priv->tzmap = cc_timezone_map_new ();
  gtk_container_add (GTK_CONTAINER (WIG ("mapBox")), GTK_WIDGET (self->priv->tzmap));
  /* Fufill the CC by Attribution license requirements for the Geonames lookup */
  cc_timezone_map_set_watermark (self->priv->tzmap, "Geonames.org");

  /* And completion entry */
  self->priv->completion = cc_timezone_completion_new ();
  cc_timezone_completion_watch_entry (self->priv->completion, GTK_ENTRY (WIG ("timezoneEntry")));
  g_signal_connect (self->priv->completion, "match-selected", G_CALLBACK (timezone_selected), self);
  g_signal_connect (WIG ("timezoneEntry"), "focus-out-event", G_CALLBACK (entry_focus_out), self);

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
  g_settings_bind (conf, SETTINGS_SHOW_DETECTED_S, WIG ("showDetectedCheck"),
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

  setup_time_spinners (self, WIG ("timeSpinner"), WIG ("dateSpinner"));

  GtkWidget * panel = WIG ("timeDatePanel");
  self->priv->auto_radio = WIG ("automaticTimeRadio");
  self->priv->tz_entry = WIG ("timezoneEntry");

  g_signal_connect_swapped (WIG ("locationsButton"), "clicked", G_CALLBACK (show_locations), self);

  /* Grab proxy for settings daemon */
  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE, NULL,
                            "org.gnome.SettingsDaemon.DateTimeMechanism",
                            "/",                            
                            "org.gnome.SettingsDaemon.DateTimeMechanism",
                            NULL, (GAsyncReadyCallback)proxy_ready, self);

  /* Grab proxy for datetime service, to see if it's running.  It would
     actually be more ideal to see if the indicator module itself is running,
     but that doesn't yet claim a name on the bus.  Presumably the service
     would have been started by any such indicator, so this will at least tell
     us if there *was* a datetime module run this session. */
  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION, G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START, NULL,
                            SERVICE_NAME, SERVICE_OBJ, SERVICE_IFACE,
                            NULL, (GAsyncReadyCallback)service_proxy_ready,
                            WIG ("showClockCheck"));

#undef WIG

  g_object_unref (conf);

  gtk_widget_show_all (panel);
  gtk_container_add (GTK_CONTAINER (self), panel);
}

static void
indicator_datetime_panel_dispose (GObject * object)
{
  IndicatorDatetimePanel * self = (IndicatorDatetimePanel *) object;
  IndicatorDatetimePanelPrivate * priv = self->priv;

  g_clear_object (&priv->builder);
  g_clear_object (&priv->proxy);

  if (priv->tz_query_cancel != NULL) {
    g_cancellable_cancel (priv->tz_query_cancel);
    g_clear_object (&priv->tz_query_cancel);
  }

  if (priv->ntp_query_cancel != NULL) {
    g_cancellable_cancel (priv->ntp_query_cancel);
    g_clear_object (&priv->ntp_query_cancel);
  }

  if (priv->loc_dlg) {
    gtk_widget_destroy (priv->loc_dlg);
    priv->loc_dlg = NULL;
  }

  if (priv->save_time_id) {
    g_source_remove (priv->save_time_id);
    priv->save_time_id = 0;
  }

  if (priv->completion) {
    cc_timezone_completion_watch_entry (priv->completion, NULL);
    g_clear_object (&priv->completion);
  }

  if (priv->tz_entry) {
    gtk_widget_destroy (priv->tz_entry);
    priv->tz_entry = NULL;
  }

  if (priv->time_spin) {
    gtk_widget_destroy (priv->time_spin);
    priv->time_spin = NULL;
  }

  if (priv->date_spin) {
    gtk_widget_destroy (priv->date_spin);
    priv->date_spin = NULL;
  }

  G_OBJECT_CLASS (indicator_datetime_panel_parent_class)->dispose (object);
}

static void
indicator_datetime_panel_class_finalize (IndicatorDatetimePanelClass *klass)
{
}

static void
indicator_datetime_panel_class_init (IndicatorDatetimePanelClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (IndicatorDatetimePanelPrivate));

  gobject_class->dispose = indicator_datetime_panel_dispose;
}

void
g_io_module_load (GIOModule *module)
{
  bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

  indicator_datetime_panel_register_type (G_TYPE_MODULE (module));
  g_io_extension_point_implement (CC_SHELL_PANEL_EXTENSION_POINT,
                                  INDICATOR_DATETIME_TYPE_PANEL,
                                  "indicator-datetime", 0);
}

void
g_io_module_unload (GIOModule *module)
{
}
