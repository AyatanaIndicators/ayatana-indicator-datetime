/*-*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
An indicator to time and date related information in the menubar.

Copyright 2010 Canonical Ltd.

Authors:
    Ted Gould <ted@canonical.com>

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

#include <config.h>
#include <libindicator/indicator-service.h>
#include <locale.h>

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <glib/gi18n.h>
#include <math.h> /* fabs() */

#include <libdbusmenu-gtk/menuitem.h>
#include <libdbusmenu-glib/server.h>
#include <libdbusmenu-glib/client.h>
#include <libdbusmenu-glib/menuitem.h>

#include <cairo/cairo.h>

#include "datetime-interface.h"
#include "dbus-shared.h"
#include "settings-shared.h"
#include "planner-eds.h"
#include "timezone-file.h"
#include "timezone-geoclue.h"
#include "utils.h"

/* how often to check for clock skew */
#define SKEW_CHECK_INTERVAL_SEC 10

#define MAX_APPOINTMENT_MENUITEMS 5

#define SKEW_DIFF_THRESHOLD_SEC (SKEW_CHECK_INTERVAL_SEC + 5)

#ifdef HAVE_CCPANEL
 #define SETTINGS_APP_INVOCATION "gnome-control-center indicator-datetime"
#else
 #define SETTINGS_APP_INVOCATION "gnome-control-center datetime"
#endif

static gboolean get_greeter_mode (void);

static void quick_set_tz (DbusmenuMenuitem * menuitem, guint timestamp, gpointer user_data);

static DbusmenuMenuitem * root = NULL;
static DatetimeInterface * dbus = NULL;

typedef struct IndicatorDatetimeService
{
  DbusmenuMenuitem * date;
  DbusmenuMenuitem * calendar;
  DbusmenuMenuitem * settings;
  DbusmenuMenuitem * events_separator;
  DbusmenuMenuitem * locations_separator;
  DbusmenuMenuitem * add_appointment;
  DbusmenuMenuitem * appointment_menuitems[MAX_APPOINTMENT_MENUITEMS];

  GSList * location_menu_items;
  gboolean updating_appointments;
  time_t start_time_appointments;
  GSettings * conf;

  IndicatorDatetimeTimezone * geo_location;
  IndicatorDatetimeTimezone * tz_file;
  IndicatorDatetimePlanner  * planner;

  guint ecaltimer;
  guint day_timer;
}
IndicatorDatetimeService;

static void update_location_menu_items    (IndicatorDatetimeService * self);
static void update_appointment_menu_items (IndicatorDatetimeService * self);
static void day_timer_reset               (IndicatorDatetimeService * self);

/**
 * A temp struct used by update_location_menu_items() for pruning duplicates and sorting.
 */
struct TimeLocation
{
  gint32 offset;
  gchar * zone;
  gchar * name;
  gboolean visible;
};

static void
time_location_free (struct TimeLocation * loc)
{
  g_free (loc->name);
  g_free (loc->zone);
  g_free (loc);
}

static struct TimeLocation*
time_location_new (const char * zone, const char * name, gboolean visible, time_t now)
{
  struct TimeLocation * loc = g_new (struct TimeLocation, 1);
  GTimeZone * tz = g_time_zone_new (zone);
  gint interval = g_time_zone_find_interval (tz, G_TIME_TYPE_UNIVERSAL, now);
  loc->offset = g_time_zone_get_offset (tz, interval);
  loc->zone = g_strdup (zone);
  loc->name = g_strdup (name);
  loc->visible = visible;
  g_time_zone_unref (tz);
  g_debug ("%s zone '%s' name '%s' offset is %d", G_STRLOC, zone, name, (int)loc->offset);
  return loc;
}

static int
time_location_compare (const struct TimeLocation * a, const struct TimeLocation * b)
{
  int ret = a->offset - b->offset; /* primary key */
  if (!ret)
    ret = g_strcmp0 (a->name, b->name); /* secondary key */
  if (!ret)
    ret = a->visible - b->visible; /* tertiary key */
  g_debug ("%s comparing '%s' (%d) to '%s' (%d), returning %d", G_STRLOC, a->name, (int)a->offset, b->name, (int)b->offset, ret);
  return ret;
}

static GSList*
locations_add (GSList * locations, const char * zone, const char * name, gboolean visible, time_t now)
{
  struct TimeLocation * loc = time_location_new (zone, name, visible, now);

  if (g_slist_find_custom (locations, loc, (GCompareFunc)time_location_compare) == NULL)
    {
      g_debug ("%s Adding zone '%s', name '%s'", G_STRLOC, zone, name);
      locations = g_slist_append (locations, loc);
    }
  else
    {
      g_debug("%s Skipping duplicate zone '%s' name '%s'", G_STRLOC, zone, name);
      time_location_free (loc);
    }

  return locations;
}

/* Update the timezone entries */
static void
update_location_menu_items (IndicatorDatetimeService * self)
{
	/* if we're in greeter mode, don't bother */
	if (self->locations_separator == NULL)
		return;

	/* remove the previous locations */
	while (self->location_menu_items != NULL) {
		DbusmenuMenuitem * item = DBUSMENU_MENUITEM(self->location_menu_items->data);
		self->location_menu_items = g_slist_remove (self->location_menu_items, item);
		dbusmenu_menuitem_child_delete (root, DBUSMENU_MENUITEM(item));
		g_object_unref (item);
	}

	/***
	****  Build a list of locations to add: use geo_timezone,
	****  current_timezone, and SETTINGS_LOCATIONS_S, but omit duplicates.
	***/

	GSList * locations = NULL;
	const time_t now = time(NULL); /* FIXME: unmockable */

	/* maybe add geo_timezone */
        if (self->geo_location != NULL) {
		const char * geo_timezone = indicator_datetime_timezone_get_timezone (self->geo_location);
		if (geo_timezone && *geo_timezone) {
			const gboolean visible = g_settings_get_boolean (self->conf, SETTINGS_SHOW_DETECTED_S);
			gchar * name = get_current_zone_name (geo_timezone);
			locations = locations_add (locations, geo_timezone, name, visible, now);
			g_free (name);
		}
	}

	/* maybe add current_timezone */
	if (self->tz_file != NULL) {
		const char * tz = indicator_datetime_timezone_get_timezone (self->tz_file);
		if (tz && *tz) {
			const gboolean visible = g_settings_get_boolean (self->conf, SETTINGS_SHOW_DETECTED_S);
			gchar * name = get_current_zone_name (tz);
			locations = locations_add (locations, tz, name, visible, now);
			g_free (name);
		}
	}

	/* maybe add the user-specified custom locations */
	gchar ** user_locations = g_settings_get_strv (self->conf, SETTINGS_LOCATIONS_S);
	if (user_locations != NULL) {
		guint i;
		const guint location_count = g_strv_length (user_locations);
		const gboolean visible = g_settings_get_boolean (self->conf, SETTINGS_SHOW_LOCATIONS_S);
		g_debug ("%s Found %u user-specified locations", G_STRLOC, location_count);
		for (i=0; i<location_count; i++) {
			gchar * zone;
			gchar * name;
			split_settings_location (user_locations[i], &zone, &name);
			locations = locations_add (locations, zone, name, visible, now);
			g_free (name);
			g_free (zone);
		}
		g_strfreev (user_locations);
		user_locations = NULL;
	}

	/* finally create menuitems for each location */
	gint offset = dbusmenu_menuitem_get_position (self->locations_separator, root)+1;
	GSList * l;
	gboolean have_visible_location = FALSE;
	for (l=locations; l!=NULL; l=l->next) {
		struct TimeLocation * loc = l->data;
		g_debug("%s Adding location: zone '%s', name '%s'", G_STRLOC, loc->zone, loc->name);
		DbusmenuMenuitem * item = dbusmenu_menuitem_new();
		dbusmenu_menuitem_property_set      (item, DBUSMENU_MENUITEM_PROP_TYPE, TIMEZONE_MENUITEM_TYPE);
		dbusmenu_menuitem_property_set      (item, TIMEZONE_MENUITEM_PROP_NAME, loc->name);
		dbusmenu_menuitem_property_set      (item, TIMEZONE_MENUITEM_PROP_ZONE, loc->zone);
		dbusmenu_menuitem_property_set_bool (item, TIMEZONE_MENUITEM_PROP_RADIO, FALSE);
		dbusmenu_menuitem_property_set_bool (item, DBUSMENU_MENUITEM_PROP_ENABLED, TRUE);
		dbusmenu_menuitem_property_set_bool (item, DBUSMENU_MENUITEM_PROP_VISIBLE, loc->visible);
		dbusmenu_menuitem_child_add_position (root, item, offset++);
		g_signal_connect(G_OBJECT(item), DBUSMENU_MENUITEM_SIGNAL_ITEM_ACTIVATED, G_CALLBACK(quick_set_tz), NULL);
		self->location_menu_items = g_slist_append (self->location_menu_items, item);
		if (loc->visible)
			have_visible_location = TRUE;
		time_location_free (loc);
	}
	g_slist_free (locations);
	locations = NULL;

	/* if there's at least one item being shown, show the separator too */
	dbusmenu_menuitem_property_set_bool (self->locations_separator, DBUSMENU_MENUITEM_PROP_VISIBLE, have_visible_location);
}

static void
quick_set_tz_cb (GObject *object, GAsyncResult *res, gpointer data G_GNUC_UNUSED)
{
  GError * error = NULL;
  GVariant * answers = g_dbus_proxy_call_finish (G_DBUS_PROXY (object), res, &error);

  if (error != NULL) {
    g_warning("Could not set timezone using timedated: %s", error->message);
    g_clear_error (&error);
    return;
  }

  g_variant_unref (answers);
}

static void
quick_set_tz_proxy_cb (GObject *object G_GNUC_UNUSED, GAsyncResult *res, gpointer zone)
{
	GError * error = NULL;

	GDBusProxy * proxy = g_dbus_proxy_new_for_bus_finish (res, &error);

	if (error != NULL) {
		g_warning("Could not grab DBus proxy for timedated: %s", error->message);
		g_clear_error (&error);
		g_free (zone);
		return;
	}

	g_dbus_proxy_call (proxy, "SetTimezone", g_variant_new ("(sb)", zone, TRUE),
	                   G_DBUS_CALL_FLAGS_NONE, -1, NULL, quick_set_tz_cb, NULL);
	g_free (zone);
	g_object_unref (proxy);
}

static void
quick_set_tz (DbusmenuMenuitem * menuitem, guint timestamp G_GNUC_UNUSED, gpointer user_data G_GNUC_UNUSED)
{
	const gchar * tz = dbusmenu_menuitem_property_get(menuitem, TIMEZONE_MENUITEM_PROP_ZONE);
	g_debug("Quick setting timezone to: %s", tz);

	g_return_if_fail(tz != NULL);

	const gchar * name = dbusmenu_menuitem_property_get(menuitem, TIMEZONE_MENUITEM_PROP_NAME);

	/* Set it in gsettings so we don't lose user's preferred name */
	GSettings * conf = g_settings_new (SETTINGS_INTERFACE);
	gchar * tz_full = g_strdup_printf ("%s %s", tz, name);
	g_settings_set_string (conf, SETTINGS_TIMEZONE_NAME_S, tz_full);
	g_free (tz_full);
	g_object_unref (conf);

	g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE, NULL,
	                          "org.freedesktop.timedate1",
	                          "/org/freedesktop/timedate1",
	                          "org.freedesktop.timedate1",
	                          NULL, quick_set_tz_proxy_cb, g_strdup (tz));

	return;
}

/* Updates the label in the date menuitem */
static gboolean
update_datetime (gpointer gself)
{
	GDateTime *datetime;
	gchar * utf8;
	IndicatorDatetimeService * self = gself;

	g_debug("Updating Date/Time");

	datetime = g_date_time_new_now_local (); /* FIXME: unmockable */
	if (datetime == NULL) {
		g_warning("Error getting local time");
		dbusmenu_menuitem_property_set(self->date, DBUSMENU_MENUITEM_PROP_LABEL, _("Error getting time"));
		return FALSE;
	}

	/* eranslators: strftime(3) style date format on top of the menu when you click on the clock */
        utf8 = g_date_time_format (datetime, _("%A, %e %B %Y"));
	dbusmenu_menuitem_property_set (self->date, DBUSMENU_MENUITEM_PROP_LABEL, utf8);
	g_free(utf8);

	g_date_time_unref (datetime);
	return G_SOURCE_REMOVE;
}

/* Run a particular program based on an activation */
static void
execute_command (const gchar * command)
{
	GError * error = NULL;

	g_debug("Issuing command '%s'", command);
	if (!g_spawn_command_line_async(command, &error)) {
		g_warning("Unable to start %s: %s", (char *)command, error->message);
		g_clear_error (&error);
	}
}

/* Run a particular program based on an activation */
static void
activate_cb (DbusmenuMenuitem  * menuitem  G_GNUC_UNUSED,
             guint               timestamp G_GNUC_UNUSED,
             const gchar       * command)
{
	execute_command (command);
}

static gboolean
update_appointment_menu_items_idle (gpointer gself)
{
  update_appointment_menu_items (gself);

  return G_SOURCE_REMOVE;
}

static void
update_appointment_menu_items_soon (IndicatorDatetimeService * self)
{
  g_idle_add (update_appointment_menu_items_idle, self);
}

static void
hide_all_appointments (IndicatorDatetimeService * self)
{
  int i;

  for (i=0; i<MAX_APPOINTMENT_MENUITEMS; i++)
    {
      if (self->appointment_menuitems[i])
        {
          dbusmenu_menuitem_property_set_bool (self->appointment_menuitems[i], DBUSMENU_MENUITEM_PROP_ENABLED, FALSE);
          dbusmenu_menuitem_property_set_bool (self->appointment_menuitems[i], DBUSMENU_MENUITEM_PROP_VISIBLE, FALSE);
        }
    }
}

static gboolean
month_changed_cb (DbusmenuMenuitem * menuitem,
                  gchar            * name G_GNUC_UNUSED,
                  GVariant         * variant,
                  guint              timestamp G_GNUC_UNUSED,
                  gpointer           gself)
{
  IndicatorDatetimeService * self = gself;

  self->start_time_appointments = (time_t)g_variant_get_uint32(variant);
	
  g_debug("Received month changed with timestamp: %d -> %s",(int)self->start_time_appointments, ctime(&self->start_time_appointments));

  /* By default, one of the first things we do is
     clear the marks as we don't know the correct
     ones yet and we don't want to confuse the user. */

  dbusmenu_menuitem_property_remove(menuitem, CALENDAR_MENUITEM_PROP_MARKS);

  update_appointment_menu_items_soon (self);
  return TRUE;
}

static gboolean
day_selected_cb (DbusmenuMenuitem * menuitem,
                 gchar            * name       G_GNUC_UNUSED,
                 GVariant         * variant,
                 guint              timestamp  G_GNUC_UNUSED,
                 gpointer           gself)
{
  time_t new_time;
  IndicatorDatetimeService * self = gself;

  new_time = (time_t)g_variant_get_uint32(variant);

  if (self->start_time_appointments == 0 || new_time == 0)
    {
      /* If we've got nothing, assume everyhting is going to
         get repopulated, let's start with a clean slate */
      dbusmenu_menuitem_property_remove (menuitem, CALENDAR_MENUITEM_PROP_MARKS);
    }
  else
    {
      /* No check to see if we changed months.  If we did we'll
         want to clear the marks.  Otherwise we're cool keeping
         them around. */
      struct tm start_tm;
      struct tm new_tm;

      localtime_r (&self->start_time_appointments, &start_tm);
      localtime_r (&new_time, &new_tm);

      if (start_tm.tm_mon != new_tm.tm_mon)
        dbusmenu_menuitem_property_remove(menuitem, CALENDAR_MENUITEM_PROP_MARKS);
    }

  self->start_time_appointments = new_time;

  g_debug ("Received day-selected with timestamp: %d -> %s",(int)self->start_time_appointments, ctime(&self->start_time_appointments));	
  update_appointment_menu_items_soon (self);

  return TRUE;
}

static gboolean
day_selected_double_click_cb (DbusmenuMenuitem * menuitem  G_GNUC_UNUSED,
                              gchar            * name      G_GNUC_UNUSED,
                              GVariant         * variant,
                              guint              timestamp G_GNUC_UNUSED,
                              gpointer           gself)
{
  time_t evotime;
  GDateTime * dt;
  IndicatorDatetimeService * self = gself;

  evotime = (time_t) g_variant_get_uint32 (variant);
  dt = g_date_time_new_from_unix_utc (evotime);
  indicator_datetime_planner_activate_time (self->planner, dt);
  g_date_time_unref (dt);

  return TRUE;
}


static gboolean
update_appointment_menu_items_timerfunc (gpointer self)
{
  update_appointment_menu_items (self);
  return G_SOURCE_CONTINUE;
}

static void
start_ecal_timer (IndicatorDatetimeService * self)
{
  if (self->ecaltimer != 0)
    self->ecaltimer = g_timeout_add_seconds (60*5, update_appointment_menu_items_timerfunc, self);
}

static void
stop_ecal_timer (IndicatorDatetimeService * self)
{
  if (self->ecaltimer != 0)
    {
      g_source_remove (self->ecaltimer);
      self->ecaltimer = 0;
    }
}

static gboolean
idle_start_ecal_timer (gpointer gself)
{
  start_ecal_timer (gself);

  return G_SOURCE_REMOVE;
}

static void
show_events_changed (IndicatorDatetimeService * self)
{
  const gboolean b = g_settings_get_boolean (self->conf, SETTINGS_SHOW_EVENTS_S);

  dbusmenu_menuitem_property_set_bool (self->add_appointment, DBUSMENU_MENUITEM_PROP_VISIBLE, b);
  dbusmenu_menuitem_property_set_bool (self->events_separator, DBUSMENU_MENUITEM_PROP_VISIBLE, b);

  if (b)
    {
      start_ecal_timer (self);
    }
  else
    {
      hide_all_appointments (self);
      stop_ecal_timer (self);
    }
}


/* Looks for the calendar application and enables the item if
   we have one, starts ecal timer if events are turned on */
static gboolean
check_for_calendar (gpointer gself)
{
	gboolean b;
	IndicatorDatetimeService * self = gself;

	g_return_val_if_fail (self->calendar != NULL, FALSE);
	
	dbusmenu_menuitem_property_set_bool(self->date, DBUSMENU_MENUITEM_PROP_ENABLED, TRUE);
	
	if (!get_greeter_mode () && indicator_datetime_planner_is_configured(self->planner)) {

		int i;
		int pos = 2;
		
		g_signal_connect (G_OBJECT(self->date), DBUSMENU_MENUITEM_SIGNAL_ITEM_ACTIVATED,
		                  G_CALLBACK (activate_cb), "evolution -c calendar");
		
		self->events_separator = dbusmenu_menuitem_new();
		dbusmenu_menuitem_property_set(self->events_separator, DBUSMENU_MENUITEM_PROP_TYPE, DBUSMENU_CLIENT_TYPES_SEPARATOR);
		dbusmenu_menuitem_child_add_position(root, self->events_separator, pos++);

		for (i=0; i<MAX_APPOINTMENT_MENUITEMS; i++)
		{
			DbusmenuMenuitem * item = dbusmenu_menuitem_new();
			dbusmenu_menuitem_property_set (item, DBUSMENU_MENUITEM_PROP_TYPE, APPOINTMENT_MENUITEM_TYPE);
			dbusmenu_menuitem_property_set_bool (item, DBUSMENU_MENUITEM_PROP_ENABLED, TRUE);
			dbusmenu_menuitem_property_set_bool (item, DBUSMENU_MENUITEM_PROP_VISIBLE, FALSE);
			self->appointment_menuitems[i] = item;
			dbusmenu_menuitem_child_add_position(root, item, pos++);
		}

		self->add_appointment = dbusmenu_menuitem_new();
		dbusmenu_menuitem_property_set (self->add_appointment, DBUSMENU_MENUITEM_PROP_LABEL, _("Add Event…"));
		dbusmenu_menuitem_property_set_bool(self->add_appointment, DBUSMENU_MENUITEM_PROP_ENABLED, TRUE);
		g_signal_connect(G_OBJECT(self->add_appointment), DBUSMENU_MENUITEM_SIGNAL_ITEM_ACTIVATED, G_CALLBACK(activate_cb), "evolution -c calendar");
		dbusmenu_menuitem_child_add_position (root, self->add_appointment, pos++);

		if (g_settings_get_boolean(self->conf, SETTINGS_SHOW_EVENTS_S)) {
			dbusmenu_menuitem_property_set_bool(self->add_appointment, DBUSMENU_MENUITEM_PROP_VISIBLE, TRUE);
			dbusmenu_menuitem_property_set_bool(self->events_separator, DBUSMENU_MENUITEM_PROP_VISIBLE, TRUE);
			g_idle_add((GSourceFunc)idle_start_ecal_timer, self);
		} else {
			dbusmenu_menuitem_property_set_bool(self->add_appointment, DBUSMENU_MENUITEM_PROP_VISIBLE, FALSE);
			dbusmenu_menuitem_property_set_bool(self->events_separator, DBUSMENU_MENUITEM_PROP_VISIBLE, FALSE);
			stop_ecal_timer (self);
		}
		
		// Connect to calendar events
		g_signal_connect(self->calendar, "event::month-changed", G_CALLBACK(month_changed_cb), self);
		g_signal_connect(self->calendar, "event::day-selected", G_CALLBACK(day_selected_cb), self);
		g_signal_connect(self->calendar, "event::day-selected-double-click", G_CALLBACK(day_selected_double_click_cb), self);
	} else {
		g_debug("Unable to find calendar app.");
		if (self->add_appointment != NULL)
			dbusmenu_menuitem_property_set_bool(self->add_appointment, DBUSMENU_MENUITEM_PROP_VISIBLE, FALSE);
		if (self->events_separator != NULL)
			dbusmenu_menuitem_property_set_bool(self->events_separator, DBUSMENU_MENUITEM_PROP_VISIBLE, FALSE);
	}
	
	b = g_settings_get_boolean (self->conf, SETTINGS_SHOW_CALENDAR_S);
	dbusmenu_menuitem_property_set_bool(self->calendar, DBUSMENU_MENUITEM_PROP_ENABLED, b);
	dbusmenu_menuitem_property_set_bool(self->calendar, DBUSMENU_MENUITEM_PROP_VISIBLE, b);

	return FALSE;
}

static GdkPixbuf *
create_color_icon_pixbuf (const char * color_spec)
{
  static int width = -1;
  static int height = -1;
  GdkPixbuf * pixbuf = NULL;

  if (width == -1)
    {
      gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &width, &height);
      width = CLAMP (width, 10, 30);
      height = CLAMP (height, 10, 30);
    }

  if (color_spec && *color_spec)
    {
      cairo_surface_t * surface;
      cairo_t * cr;
      GdkRGBA rgba;

      surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
      cr = cairo_create (surface);

      if (gdk_rgba_parse (&rgba, color_spec))
        gdk_cairo_set_source_rgba (cr, &rgba);

      cairo_paint (cr);
      cairo_set_source_rgba (cr, 0, 0, 0, 0.5);
      cairo_set_line_width (cr, 1);
      cairo_rectangle (cr, 0.5, 0.5, width-1, height-1);
      cairo_stroke (cr);

      pixbuf = gdk_pixbuf_get_from_surface (surface, 0, 0, width, height);

      cairo_destroy (cr);
      cairo_surface_destroy (surface);
    }

  return pixbuf;
}


/**
 * Populate the menu with todays, next MAX_APPOINTMENT_MENUITEMS appointments.
 * we should hook into the ABOUT TO SHOW signal and use that to update the appointments.
 * Experience has shown that caldav's and webcals can be slow to load from eds
 * this is a problem mainly on the EDS side of things, not ours.
 */
static void
update_appointment_menu_items (IndicatorDatetimeService * self)
{
  char * str;
  GSList * l;
  GSList * appointments;
  gint i;
  GDateTime * begin;
  GDateTime * end;
  GdkPixbuf * pixbuf;
  gint apt_output;
  GVariantBuilder markeddays;
  GVariant * marks;

  // FFR: we should take into account short term timers, for instance
  // tea timers, pomodoro timers etc... that people may add, this is hinted to in the spec.

  g_debug ("Update appointments called");

  if (self->calendar == NULL)
    return;
  if (!g_settings_get_boolean (self->conf, SETTINGS_SHOW_EVENTS_S))
    return;
  if (self->updating_appointments)
    return;

  self->updating_appointments = TRUE;
	
  g_variant_builder_init (&markeddays, G_VARIANT_TYPE ("ai"));

  if (self->start_time_appointments != 0)
    begin = g_date_time_new_from_unix_local (self->start_time_appointments);
  else
    begin = g_date_time_new_now_local (); /* FIXME: unmockable */
  end = g_date_time_add_months (begin, 1);
  indicator_datetime_planner_set_timezone (self->planner, indicator_datetime_timezone_get_timezone (self->tz_file));
  appointments = indicator_datetime_planner_get_appointments (self->planner, begin, end);

  hide_all_appointments (self);

  /* decide whether to use 12hr or 24hr format */
  str = g_settings_get_string (self->conf, SETTINGS_TIME_FORMAT_S);
  if (g_strcmp0 (str, "12-hour") == 0)
    apt_output = SETTINGS_TIME_12_HOUR;
  else if (g_strcmp0 (str, "24-hour") == 0)
    apt_output = SETTINGS_TIME_24_HOUR;
  else if (is_locale_12h())
    apt_output = SETTINGS_TIME_12_HOUR;
  else
    apt_output = SETTINGS_TIME_24_HOUR;
  g_free (str);
	
  i = 0;
  for (l=appointments; l!=NULL; l=l->next)
    {
      GDateTime * due;
      DbusmenuMenuitem * item;
      const struct IndicatorDatetimeAppt * appt = l->data;
      char * right = NULL;

      due = appt->is_event ? appt->begin : appt->end;

      /* mark day if our query hasn't hit the next month. */
      if (g_date_time_get_month (begin) == g_date_time_get_month (due))
        g_variant_builder_add (&markeddays, "i", g_date_time_get_day_of_month (due));

      if (i >= MAX_APPOINTMENT_MENUITEMS)
        continue;

      item = self->appointment_menuitems[i];
      i++;

      /* remove the icon as we might not replace it on error */
      dbusmenu_menuitem_property_remove(item, APPOINTMENT_MENUITEM_PROP_ICON);

      /* remove the activate handler */
      g_signal_handlers_disconnect_matched(G_OBJECT(item), G_SIGNAL_MATCH_FUNC, 0, 0, NULL, G_CALLBACK(activate_cb), NULL);

      dbusmenu_menuitem_property_set_bool (item, DBUSMENU_MENUITEM_PROP_ENABLED, TRUE);
      dbusmenu_menuitem_property_set_bool (item, DBUSMENU_MENUITEM_PROP_VISIBLE, TRUE);
      dbusmenu_menuitem_property_set (item, APPOINTMENT_MENUITEM_PROP_LABEL, appt->summary);

      gboolean full_day = FALSE;
      if (appt->is_event)
        full_day = g_date_time_difference (appt->end, appt->begin) == G_TIME_SPAN_DAY;

      if (full_day)
        {
          /* TRANSLATORS: This is a strftime string for the day for full day events
             in the menu.  It should most likely be either '%A' for a full text day
             (Wednesday) or '%a' for a shortened one (Wed).  You should only need to
             change for '%a' in the case of languages with very long day names. */
          right = g_date_time_format (due, _("%A"));
        }
      else
        {
          int ay, am, ad;
          int by, bm, bd;
          gboolean same_day;
          gboolean hr12;
          const char * fmt;

          g_date_time_get_ymd (due, &ay, &am, &ad);
          g_date_time_get_ymd (begin, &by, &bm, &bd);
          same_day = (ay==by) && (am==bm) && (ad==bd);
          hr12 = apt_output == SETTINGS_TIME_12_HOUR;

          if (same_day && hr12)
            fmt = _(DEFAULT_TIME_12_FORMAT);
          else if (same_day)
            fmt = _(DEFAULT_TIME_24_FORMAT);
          else if (hr12)
            fmt = _(DEFAULT_TIME_12_FORMAT_WITH_DAY);
          else
            fmt = _(DEFAULT_TIME_24_FORMAT_WITH_DAY);

          right = g_date_time_format (due, fmt);
        }

      dbusmenu_menuitem_property_set (item, APPOINTMENT_MENUITEM_PROP_RIGHT, right);
      g_free (right);
		
      // Now we pull out the URI for the calendar event and try to create a URI that'll work when we execute evolution
      // FIXME Because the URI stuff is really broken, we're going to open the calendar at todays date instead
      //e_cal_component_get_uid(ecalcomp, &uri);
/* FIXME: appointment menuitems aren't clickable */
#if 0
      gchar * ad = isodate_from_time_t(mktime(due));
      gchar * cmd = g_strconcat("evolution calendar:///?startdate=", ad, NULL);
      g_debug("Command to Execute: %s", cmd);
      g_signal_connect_data (G_OBJECT(item), DBUSMENU_MENUITEM_SIGNAL_ITEM_ACTIVATED,
                             G_CALLBACK(activate_cb), cmd, (GClosureNotify)g_free, 0);
      g_free (ad);
#endif

      if ((pixbuf = create_color_icon_pixbuf (appt->color)))
        {
          dbusmenu_menuitem_property_set_image (item, APPOINTMENT_MENUITEM_PROP_ICON, pixbuf);
          g_clear_object (&pixbuf);
        }
    }

	
  marks = g_variant_builder_end (&markeddays);
  dbusmenu_menuitem_property_set_variant (self->calendar, CALENDAR_MENUITEM_PROP_MARKS, marks);

  g_slist_free_full (appointments, (GDestroyNotify)indicator_datetime_appt_free);
	
  self->updating_appointments = FALSE;
  g_date_time_unref (end);
  g_date_time_unref (begin);
}

/* Looks for the time and date admin application and enables the
   item we have one */
static gboolean
check_for_timeadmin (gpointer gself)
{
  gchar * timeadmin;
  IndicatorDatetimeService * self = gself;

  g_return_val_if_fail (self->settings != NULL, FALSE);

  timeadmin = g_find_program_in_path ("gnome-control-center");
  if (timeadmin != NULL)
    {
      g_debug ("Found the gnome-control-center application: %s", timeadmin);
      dbusmenu_menuitem_property_set_bool (self->settings, DBUSMENU_MENUITEM_PROP_ENABLED, TRUE);
      g_free(timeadmin);
    }
  else
    {
      g_debug ("Unable to find gnome-control-center app.");
      dbusmenu_menuitem_property_set_bool (self->settings, DBUSMENU_MENUITEM_PROP_ENABLED, FALSE);
    }

  return G_SOURCE_REMOVE;
}

/* Does the work to build the default menu, really calls out
   to other functions but this is the core to clean up the
   main function. */
static void
build_menus (IndicatorDatetimeService * self, DbusmenuMenuitem * root)
{
	g_debug("Building Menus.");
	if (self->date == NULL) {
		self->date = dbusmenu_menuitem_new();
		dbusmenu_menuitem_property_set     (self->date, DBUSMENU_MENUITEM_PROP_LABEL, _("No date yet…"));
		dbusmenu_menuitem_property_set_bool(self->date, DBUSMENU_MENUITEM_PROP_ENABLED, FALSE);
		dbusmenu_menuitem_child_append(root, self->date);

		g_idle_add(update_datetime, self);
	}

	if (self->calendar == NULL) {
		self->calendar = dbusmenu_menuitem_new();
		dbusmenu_menuitem_property_set (self->calendar, DBUSMENU_MENUITEM_PROP_TYPE, DBUSMENU_CALENDAR_MENUITEM_TYPE);
		/* insensitive until we check for available apps */
		dbusmenu_menuitem_property_set_bool(self->calendar, DBUSMENU_MENUITEM_PROP_ENABLED, FALSE);
		g_signal_connect (G_OBJECT(self->calendar), DBUSMENU_MENUITEM_SIGNAL_ITEM_ACTIVATED,
						  G_CALLBACK (activate_cb), "evolution -c calendar");
		dbusmenu_menuitem_child_append(root, self->calendar);

		g_idle_add(check_for_calendar, self);
	}

	if (!get_greeter_mode ()) {
		self->locations_separator = dbusmenu_menuitem_new();
		dbusmenu_menuitem_property_set(self->locations_separator, DBUSMENU_MENUITEM_PROP_TYPE, DBUSMENU_CLIENT_TYPES_SEPARATOR);
		dbusmenu_menuitem_property_set_bool (self->locations_separator, DBUSMENU_MENUITEM_PROP_VISIBLE, FALSE);
		dbusmenu_menuitem_child_append(root, self->locations_separator);

		update_location_menu_items (self);
	
		g_signal_connect (self->conf, "changed::" SETTINGS_SHOW_EVENTS_S, G_CALLBACK (show_events_changed), self);
		g_signal_connect_swapped (self->conf, "changed::" SETTINGS_SHOW_LOCATIONS_S, G_CALLBACK (update_location_menu_items), self);
		g_signal_connect_swapped (self->conf, "changed::" SETTINGS_SHOW_DETECTED_S,  G_CALLBACK (update_location_menu_items), self);
		g_signal_connect_swapped (self->conf, "changed::" SETTINGS_LOCATIONS_S,      G_CALLBACK (update_location_menu_items), self);
		g_signal_connect_swapped (self->conf, "changed::" SETTINGS_TIME_FORMAT_S,    G_CALLBACK (update_appointment_menu_items), self);

		DbusmenuMenuitem * separator = dbusmenu_menuitem_new();
		dbusmenu_menuitem_property_set(separator, DBUSMENU_MENUITEM_PROP_TYPE, DBUSMENU_CLIENT_TYPES_SEPARATOR);
		dbusmenu_menuitem_child_append(root, separator);

		self->settings = dbusmenu_menuitem_new();
		dbusmenu_menuitem_property_set     (self->settings, DBUSMENU_MENUITEM_PROP_LABEL, _("Time & Date Settings…"));
		/* insensitive until we check for available apps */
		dbusmenu_menuitem_property_set_bool(self->settings, DBUSMENU_MENUITEM_PROP_ENABLED, FALSE);
		g_signal_connect(G_OBJECT(self->settings), DBUSMENU_MENUITEM_SIGNAL_ITEM_ACTIVATED, G_CALLBACK(activate_cb), SETTINGS_APP_INVOCATION);
		dbusmenu_menuitem_child_append(root, self->settings);
		g_idle_add(check_for_timeadmin, self);
		update_appointment_menu_items_soon (self);
	}

	return;
}

static void
on_clock_skew (gpointer self)
{
  /* tell the indicators to refresh */
  if (IS_DATETIME_INTERFACE (dbus))
    datetime_interface_update (DATETIME_INTERFACE(dbus));

  /* update our day label */
  update_datetime (self);
  day_timer_reset (self);
}

static void
on_timezone_changed (gpointer self)
{
  update_location_menu_items (self);

  on_clock_skew (self);
}

/* Execute at a given time, update and setup a new
   timer to go again.  */
static gboolean
day_timer_func (gpointer self)
{
  day_timer_reset (self);
  update_datetime (self);

  return G_SOURCE_REMOVE;
}

/* Sets up the time to launch the timer to update the
   date in the datetime entry */
static void
day_timer_reset (IndicatorDatetimeService * self)
{
  GDateTime * now;
  GDateTime * tomorrow;
  GDateTime * new_day;
  guint seconds_until_tomorrow;

  if (self->day_timer != 0)
    {
      g_source_remove (self->day_timer);
      self->day_timer = 0;
    }

  now = g_date_time_new_now_local ();
  tomorrow = g_date_time_add_days (now, 1);
  new_day = g_date_time_new_local (g_date_time_get_year (tomorrow),
                                   g_date_time_get_month (tomorrow),
                                   g_date_time_get_day_of_month (tomorrow),
                                   0, 0, 0);
  seconds_until_tomorrow = (guint)(g_date_time_difference (new_day, now) / G_TIME_SPAN_SECOND);
g_message ("seconds until tomorrow is %u", seconds_until_tomorrow);

  self->day_timer = g_timeout_add_seconds (seconds_until_tomorrow + 15,
                                           day_timer_func,
                                           self);

  g_date_time_unref (new_day);
  g_date_time_unref (tomorrow);
  g_date_time_unref (now);
}

static gboolean
skew_check_timer_func (gpointer self)
{
  static time_t prev_time = 0;
  const time_t cur_time = time (NULL); /* FIXME: unmockable */
  const double diff_sec = fabs (difftime (cur_time, prev_time));

  if (prev_time && (diff_sec > SKEW_DIFF_THRESHOLD_SEC))
    {
      g_debug (G_STRLOC" clock skew detected (%.0f seconds)", diff_sec);
      on_clock_skew (self);
    }

  prev_time = cur_time;
  return G_SOURCE_CONTINUE;
}

static void
session_active_change_cb (GDBusProxy * proxy        G_GNUC_UNUSED,
                          gchar      * sender_name  G_GNUC_UNUSED,
                          gchar      * signal_name,
                          GVariant   * parameters,
                          gpointer     gself)
{
  IndicatorDatetimeService * self = gself;

  /* suspending / returning from suspend (true / false) */
  if (g_strcmp0(signal_name, "PrepareForSleep") == 0)
    {
      gboolean sleeping = FALSE;
      g_variant_get (parameters, "(b)", &sleeping);
      if (!sleeping)
        {
          g_debug ("System has been resumed; adjusting clock");
          on_clock_skew (self);
        }
    }
}

/* for hooking into console kit signal on wake from suspend */
static void
system_proxy_cb (GObject       * object G_GNUC_UNUSED,
                 GAsyncResult  * res,
                 gpointer        gself)
{
	GError * error = NULL;
	
	GDBusProxy * proxy = g_dbus_proxy_new_for_bus_finish(res, &error);

	if (error != NULL) {
		g_warning("Could not grab DBus proxy for logind: %s", error->message);
		g_clear_error (&error);
		return;
	}

	g_signal_connect(proxy, "g-signal", G_CALLBACK(session_active_change_cb), gself);
}

/****
*****
****/

static gboolean
get_greeter_mode (void)
{
  const gchar *var;
  var = g_getenv("INDICATOR_GREETER_MODE");
  return (g_strcmp0(var, "1") == 0);
}

/* Repsonds to the service object saying it's time to shutdown.
   It stops the mainloop. */
static void
service_shutdown (IndicatorService * service    G_GNUC_UNUSED,
                  gpointer           gmainloop)
{
  g_warning ("Shutting down service!");
  g_main_loop_quit (gmainloop);
}

static void
on_use_geoclue_changed_cb (GSettings * settings,
                           gchar     * key       G_GNUC_UNUSED,
                           gpointer    gself)
{
  IndicatorDatetimeService * self = gself;
  const gboolean use_geoclue = g_settings_get_boolean (settings, "show-auto-detected-location");

  if (self->geo_location && !use_geoclue)
    {
      g_signal_handlers_disconnect_by_func (self->geo_location, update_location_menu_items, self);
      g_clear_object (&self->geo_location);
      update_location_menu_items (self);
    }
  else if (use_geoclue && !self->geo_location)
    {
      self->geo_location = indicator_datetime_timezone_geoclue_new ();
      g_signal_connect_swapped (self->geo_location, "notify::timezone",
                                G_CALLBACK(update_location_menu_items), self);
    }
}

/* Function to build everything up.  Entry point from asm. */
int
main (int argc, char ** argv)
{
  GMainLoop * mainloop;
  IndicatorService * service;
  DbusmenuServer * server;
  struct IndicatorDatetimeService self;

  memset (&self, 0, sizeof(struct IndicatorDatetimeService));

  gtk_init (&argc, &argv);
  mainloop = g_main_loop_new (NULL, FALSE);

  /* acknowledging the service init and setting up the interface */
  service = indicator_service_new_version(SERVICE_NAME, SERVICE_VERSION);
  g_signal_connect (service, INDICATOR_SERVICE_SIGNAL_SHUTDOWN,
                    G_CALLBACK(service_shutdown), mainloop);

  /* setting up i18n and gettext. apparently we need all of these. */
  setlocale (LC_ALL, "");
  bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
  textdomain (GETTEXT_PACKAGE);

  /* set up GSettings */
  self.conf = g_settings_new (SETTINGS_INTERFACE);
  g_signal_connect (self.conf, "changed::show-auto-detected-location",
                    G_CALLBACK(on_use_geoclue_changed_cb), &self);

  /* setup geoclue */
  on_use_geoclue_changed_cb (self.conf, NULL, &self);

  /* setup timezone watch */
  self.tz_file = indicator_datetime_timezone_file_new (TIMEZONE_FILE);
  g_signal_connect_swapped (self.tz_file, "notify::timezone",
                            G_CALLBACK(on_timezone_changed), &self);

  /* build our list of appointment calendar sources.
     When a source changes, update our menu items.
     When sources are added or removed, update our list and menu items. */
  self.planner = indicator_datetime_planner_eds_new ();
  g_signal_connect_swapped (self.planner, "appointments-changed",
                            G_CALLBACK(update_appointment_menu_items_soon), &self);

  /* building the base menu */
  server = dbusmenu_server_new (MENU_OBJ);
  root = dbusmenu_menuitem_new ();
  dbusmenu_server_set_root (server, root);
	
  build_menus (&self, root);
	
  /* cache the timezone */
  update_location_menu_items (&self);

  /* setup dbus interface */
  dbus = g_object_new (DATETIME_INTERFACE_TYPE, NULL);

  /* set up the day timer */
  day_timer_reset (&self);

  /* set up the skew-check timer */
  g_timeout_add_seconds (SKEW_CHECK_INTERVAL_SEC,
                         skew_check_timer_func,
                         &self);

  /* and watch for system resumes */
  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                            G_DBUS_PROXY_FLAGS_NONE,
                            NULL,
                            "org.freedesktop.login1",
                            "/org/freedesktop/login1",
                            "org.freedesktop.login1.Manager",
                            NULL, /* FIXME: cancellable */
                            system_proxy_cb,
                            &self);

  g_main_loop_run (mainloop);
  g_main_loop_unref (mainloop);

  g_object_unref (self.conf);
  g_object_unref (dbus);
  g_object_unref (service);
  g_object_unref (server);
  g_object_unref (root);
  g_object_unref (self.planner);
  g_object_unref (self.geo_location);
  g_object_unref (self.tz_file);

  return 0;
}
