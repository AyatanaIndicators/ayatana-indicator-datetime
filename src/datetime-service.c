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
#include <gio/gio.h>
#include <math.h>
#include <gconf/gconf-client.h>

#include <libdbusmenu-gtk/menuitem.h>
#include <libdbusmenu-glib/server.h>
#include <libdbusmenu-glib/client.h>
#include <libdbusmenu-glib/menuitem.h>

#include <geoclue/geoclue-master.h>
#include <geoclue/geoclue-master-client.h>

#include <time.h>
#include <libecal/e-cal.h>
#include <libical/ical.h>
#include <libecal/e-cal-time-util.h>
#include <libedataserver/e-source.h>
#include <libedataserverui/e-passwords.h>
// Other users of ecal seem to also include these, not sure why they should be included by the above
#include <libical/icaltime.h>
#include <cairo/cairo.h>

#include "datetime-interface.h"
#include "dbus-shared.h"
#include "settings-shared.h"
#include "utils.h"

#ifdef HAVE_CCPANEL
 #define SETTINGS_APP_INVOCATION "gnome-control-center indicator-datetime"
#else
 #define SETTINGS_APP_INVOCATION "gnome-control-center datetime"
#endif

static void geo_create_client (GeoclueMaster * master, GeoclueMasterClient * client, gchar * path, GError * error, gpointer user_data);
static gboolean update_appointment_menu_items (gpointer user_data);
static void update_location_menu_items (void);
static void setup_timer (void);
static void geo_client_invalid (GeoclueMasterClient * client, gpointer user_data);
static void geo_address_change (GeoclueMasterClient * client, gchar * a, gchar * b, gchar * c, gchar * d, gpointer user_data);
static gboolean get_greeter_mode (void);

static void quick_set_tz (DbusmenuMenuitem * menuitem, guint timestamp, gpointer user_data);

static IndicatorService * service = NULL;
static GMainLoop * mainloop = NULL;
static DbusmenuServer * server = NULL;
static DbusmenuMenuitem * root = NULL;
static DatetimeInterface * dbus = NULL;

/* Global Items */
static DbusmenuMenuitem * date = NULL;
static DbusmenuMenuitem * calendar = NULL;
static DbusmenuMenuitem * settings = NULL;
static DbusmenuMenuitem * events_separator = NULL;
static DbusmenuMenuitem * locations_separator = NULL;
static DbusmenuMenuitem * add_appointment = NULL;
static GList            * appointments = NULL;
static GSList           * location_menu_items = NULL;
static GList            * comp_instances = NULL;
static gboolean           updating_appointments = FALSE;
static time_t             start_time_appointments = (time_t) 0;
static GSettings        * conf = NULL;
static GConfClient      * gconf = NULL;


/* Geoclue trackers */
static GeoclueMasterClient * geo_master = NULL;
static GeoclueAddress * geo_address = NULL;

/* Our 2 important timezones */
static gchar 			* current_timezone = NULL;
static gchar 			* geo_timezone = NULL;

struct comp_instance {
        ECalComponent *comp;
        time_t start;
        time_t end;
        ESource *source;
};

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

	if (g_slist_find_custom (locations, loc, (GCompareFunc)time_location_compare) == NULL) {
		g_debug ("%s Adding zone '%s', name '%s'", G_STRLOC, zone, name);
		locations = g_slist_append (locations, loc);
	} else {
		g_debug("%s Skipping duplicate zone '%s' name '%s'", G_STRLOC, zone, name);
		time_location_free (loc);
	}
	return locations;
}

/* Update the timezone entries */
static void
update_location_menu_items (void)
{
	/* if we're in greeter mode, don't bother */
	if (locations_separator == NULL)
		return;

	/* remove the previous locations */
	while (location_menu_items != NULL) {
		DbusmenuMenuitem * item = DBUSMENU_MENUITEM(location_menu_items->data);
		location_menu_items = g_slist_remove(location_menu_items, item);
		dbusmenu_menuitem_child_delete(root, DBUSMENU_MENUITEM(item));
		g_object_unref(G_OBJECT(item));
	}

	/***
	****  Build a list of locations to add: use geo_timezone,
	****  current_timezone, and SETTINGS_LOCATIONS_S, but omit duplicates.
	***/

	GSList * locations = NULL;
	const time_t now = time(NULL);

	/* maybe add geo_timezone */
	if (geo_timezone != NULL) {
		const gboolean visible = g_settings_get_boolean (conf, SETTINGS_SHOW_DETECTED_S);
		gchar * name = get_current_zone_name (geo_timezone);
		locations = locations_add (locations, geo_timezone, name, visible, now);
		g_free (name);
	}

	/* maybe add current_timezone */
	if (current_timezone != NULL) {
		const gboolean visible = g_settings_get_boolean (conf, SETTINGS_SHOW_DETECTED_S);
		gchar * name = get_current_zone_name (current_timezone);
		locations = locations_add (locations, current_timezone, name, visible, now);
		g_free (name);
	}

	/* maybe add the user-specified custom locations */
	gchar ** user_locations = g_settings_get_strv (conf, SETTINGS_LOCATIONS_S);
	if (user_locations != NULL) { 
		gint i;
		const guint location_count = g_strv_length (user_locations);
		const gboolean visible = g_settings_get_boolean (conf, SETTINGS_SHOW_LOCATIONS_S);
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
	gint offset = dbusmenu_menuitem_get_position (locations_separator, root)+1;
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
		location_menu_items = g_slist_append (location_menu_items, item);
		if (loc->visible)
			have_visible_location = TRUE;
		time_location_free (loc);
	}
	g_slist_free (locations);
	locations = NULL;

	/* if there's at least one item being shown, show the separator too */
	dbusmenu_menuitem_property_set_bool (locations_separator, DBUSMENU_MENUITEM_PROP_VISIBLE, have_visible_location);
}

/* Update the current timezone */
static void
update_current_timezone (void) {
	/* Clear old data */
	if (current_timezone != NULL) {
		g_free(current_timezone);
		current_timezone = NULL;
	}

	current_timezone = read_timezone ();
	if (current_timezone == NULL) {
		return;
	}

	g_debug("System timezone is: %s", current_timezone);

	update_location_menu_items();

	return;
}

static void
quick_set_tz_cb (GObject *object, GAsyncResult *res, gpointer data)
{
  GError * error = NULL;
  GVariant * answers = g_dbus_proxy_call_finish (G_DBUS_PROXY (object), res, &error);

  if (error != NULL) {
    g_warning("Could not set timezone for SettingsDaemon: %s", error->message);
    g_clear_error (&error);
    return;
  }

  g_variant_unref (answers);
}

static void
quick_set_tz_proxy_cb (GObject *object, GAsyncResult *res, gpointer zone)
{
	GError * error = NULL;

	GDBusProxy * proxy = g_dbus_proxy_new_for_bus_finish (res, &error);

	if (error != NULL) {
		g_warning("Could not grab DBus proxy for SettingsDaemon: %s", error->message);
		g_clear_error (&error);
		g_free (zone);
		return;
	}

	g_dbus_proxy_call (proxy, "SetTimezone", g_variant_new ("(s)", zone),
	                   G_DBUS_CALL_FLAGS_NONE, -1, NULL, quick_set_tz_cb, NULL);
	g_free (zone);
	g_object_unref (proxy);
}

static void
quick_set_tz (DbusmenuMenuitem * menuitem, guint timestamp, gpointer user_data)
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
	                          "org.gnome.SettingsDaemon.DateTimeMechanism",
	                          "/",                            
	                          "org.gnome.SettingsDaemon.DateTimeMechanism",
	                          NULL, quick_set_tz_proxy_cb, g_strdup (tz));

	return;
}

/* Updates the label in the date menuitem */
static gboolean
update_datetime (gpointer user_data)
{
	GDateTime *datetime;
	gchar *utf8;

	g_debug("Updating Date/Time");

	datetime = g_date_time_new_now_local ();
	if (datetime == NULL) {
		g_warning("Error getting local time");
		dbusmenu_menuitem_property_set(date, DBUSMENU_MENUITEM_PROP_LABEL, _("Error getting time"));
		g_date_time_unref (datetime);
		return FALSE;
	}

	/* eranslators: strftime(3) style date format on top of the menu when you click on the clock */
        utf8 = g_date_time_format (datetime, _("%A, %e %B %Y"));

	dbusmenu_menuitem_property_set(date, DBUSMENU_MENUITEM_PROP_LABEL, utf8);

	g_date_time_unref (datetime);
	g_free(utf8);

	return FALSE;
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
update_appointment_menu_items_idle (gpointer user_data)
{
	update_appointment_menu_items(user_data);
	return FALSE;
}

static gboolean
month_changed_cb (DbusmenuMenuitem * menuitem, gchar *name, GVariant *variant, guint timestamp)
{
	start_time_appointments = (time_t)g_variant_get_uint32(variant);
	
	g_debug("Received month changed with timestamp: %d -> %s",(int)start_time_appointments, ctime(&start_time_appointments));	
	/* By default one of the first things we do is
	   clear the marks as we don't know the correct
	   ones yet and we don't want to confuse the
	   user. */
	dbusmenu_menuitem_property_remove(menuitem, CALENDAR_MENUITEM_PROP_MARKS);

	GList * appointment;
	for (appointment = appointments; appointment != NULL; appointment = g_list_next(appointment)) {
		DbusmenuMenuitem * mi = DBUSMENU_MENUITEM(appointment->data);
		dbusmenu_menuitem_property_set_bool(mi, DBUSMENU_MENUITEM_PROP_ENABLED, FALSE);
	}

	g_idle_add(update_appointment_menu_items_idle, NULL);
	return TRUE;
}

static gboolean
day_selected_cb (DbusmenuMenuitem * menuitem, gchar *name, GVariant *variant, guint timestamp)
{
	time_t new_time = (time_t)g_variant_get_uint32(variant);
	g_warn_if_fail(new_time != 0);

	if (start_time_appointments == 0 || new_time == 0) {
		/* If we've got nothing, assume everyhting is going to
		   get repopulated, let's start with a clean slate */
		dbusmenu_menuitem_property_remove(menuitem, CALENDAR_MENUITEM_PROP_MARKS);
	} else {
		/* No check to see if we changed months.  If we did we'll
		   want to clear the marks.  Otherwise we're cool keeping
		   them around. */
		struct tm start_tm;
		struct tm new_tm;

		localtime_r(&start_time_appointments, &start_tm);
		localtime_r(&new_time, &new_tm);

		if (start_tm.tm_mon != new_tm.tm_mon) {
			dbusmenu_menuitem_property_remove(menuitem, CALENDAR_MENUITEM_PROP_MARKS);
		}
	}

	GList * appointment;
	for (appointment = appointments; appointment != NULL; appointment = g_list_next(appointment)) {
		DbusmenuMenuitem * mi = DBUSMENU_MENUITEM(appointment->data);
		dbusmenu_menuitem_property_set_bool(mi, DBUSMENU_MENUITEM_PROP_ENABLED, FALSE);
	}

	start_time_appointments = new_time;

	g_debug("Received day-selected with timestamp: %d -> %s",(int)start_time_appointments, ctime(&start_time_appointments));	
	g_idle_add(update_appointment_menu_items_idle, NULL);

	return TRUE;
}

static gboolean
day_selected_double_click_cb (DbusmenuMenuitem * menuitem  G_GNUC_UNUSED,
                              gchar            * name      G_GNUC_UNUSED,
                              GVariant         * variant,
                              guint              timestamp G_GNUC_UNUSED)
{
	const time_t evotime = (time_t)g_variant_get_uint32(variant);
	
	g_debug("Received day-selected-double-click with timestamp: %d -> %s",(int)evotime, ctime(&evotime));	
	
	gchar *ad = isodate_from_time_t(evotime);
	gchar *cmd = g_strconcat("evolution calendar:///?startdate=", ad, NULL);
	
	execute_command (cmd);

	g_free (cmd);
	g_free (ad);
	
	return TRUE;
}

static guint ecaltimer = 0;

static void
start_ecal_timer(void)
{
	if (ecaltimer != 0) {
		g_source_remove(ecaltimer);
		ecaltimer = 0;
	}
	if (update_appointment_menu_items(NULL))
		ecaltimer = g_timeout_add_seconds(60*5, update_appointment_menu_items, NULL); 	
}

static void
stop_ecal_timer(void)
{
	if (ecaltimer != 0) {
		g_source_remove(ecaltimer);
		ecaltimer = 0;
	}
}
static gboolean
idle_start_ecal_timer (gpointer data)
{
	start_ecal_timer();
	return FALSE;
}

static void
show_events_changed (void)
{
	if (g_settings_get_boolean(conf, SETTINGS_SHOW_EVENTS_S)) {
		dbusmenu_menuitem_property_set_bool(add_appointment, DBUSMENU_MENUITEM_PROP_VISIBLE, TRUE);
		dbusmenu_menuitem_property_set_bool(events_separator, DBUSMENU_MENUITEM_PROP_VISIBLE, TRUE);
		start_ecal_timer();
	} else {
		dbusmenu_menuitem_property_set_bool(add_appointment, DBUSMENU_MENUITEM_PROP_VISIBLE, FALSE);
		dbusmenu_menuitem_property_set_bool(events_separator, DBUSMENU_MENUITEM_PROP_VISIBLE, FALSE);
		/* Remove all of the previous appointments */
		if (appointments != NULL) {
			g_debug("Hiding old appointments");
			GList * appointment;
			for (appointment = appointments; appointment != NULL; appointment = g_list_next(appointment)) {
				DbusmenuMenuitem * litem =  DBUSMENU_MENUITEM(appointment->data);
				g_debug("Hiding old appointment: %p", litem);
				// Remove all the existing menu items which are in appointments.
				dbusmenu_menuitem_property_set_bool(DBUSMENU_MENUITEM(litem), DBUSMENU_MENUITEM_PROP_VISIBLE, FALSE);
			}
		}
		stop_ecal_timer();
	}
}

static gboolean
calendar_app_is_usable (void)
{
	/* confirm that it's installed... */
	gchar *evo = g_find_program_in_path("evolution");
	if (evo == NULL)
		return FALSE;
	g_debug ("found calendar app: '%s'", evo);
	g_free (evo);

	/* confirm that it's got an account set up... */
	GSList *accounts_list = gconf_client_get_list (gconf, "/apps/evolution/mail/accounts", GCONF_VALUE_STRING, NULL);
	const guint n = g_slist_length (accounts_list);
	g_debug ("found %u evolution accounts", n);
	g_slist_free_full (accounts_list, g_free);
	return n > 0;
}

/* Looks for the calendar application and enables the item if
   we have one, starts ecal timer if events are turned on */
static gboolean
check_for_calendar (gpointer user_data)
{
	g_return_val_if_fail (calendar != NULL, FALSE);
	
	dbusmenu_menuitem_property_set_bool(date, DBUSMENU_MENUITEM_PROP_ENABLED, TRUE);
	
	if (!get_greeter_mode () && calendar_app_is_usable()) {
		
		g_signal_connect (G_OBJECT(date), DBUSMENU_MENUITEM_SIGNAL_ITEM_ACTIVATED,
		                  G_CALLBACK (activate_cb), "evolution -c calendar");
		
		events_separator = dbusmenu_menuitem_new();
		dbusmenu_menuitem_property_set(events_separator, DBUSMENU_MENUITEM_PROP_TYPE, DBUSMENU_CLIENT_TYPES_SEPARATOR);
		dbusmenu_menuitem_child_add_position(root, events_separator, 2);
		add_appointment = dbusmenu_menuitem_new();
		dbusmenu_menuitem_property_set (add_appointment, DBUSMENU_MENUITEM_PROP_LABEL, _("Add Event…"));
		dbusmenu_menuitem_property_set_bool(add_appointment, DBUSMENU_MENUITEM_PROP_ENABLED, TRUE);
		g_signal_connect(G_OBJECT(add_appointment), DBUSMENU_MENUITEM_SIGNAL_ITEM_ACTIVATED, G_CALLBACK(activate_cb), "evolution -c calendar");
		dbusmenu_menuitem_child_add_position (root, add_appointment, 3);


		if (g_settings_get_boolean(conf, SETTINGS_SHOW_EVENTS_S)) {
			dbusmenu_menuitem_property_set_bool(add_appointment, DBUSMENU_MENUITEM_PROP_VISIBLE, TRUE);
			dbusmenu_menuitem_property_set_bool(events_separator, DBUSMENU_MENUITEM_PROP_VISIBLE, TRUE);
			g_idle_add((GSourceFunc)idle_start_ecal_timer, NULL);
		} else {
			dbusmenu_menuitem_property_set_bool(add_appointment, DBUSMENU_MENUITEM_PROP_VISIBLE, FALSE);
			dbusmenu_menuitem_property_set_bool(events_separator, DBUSMENU_MENUITEM_PROP_VISIBLE, FALSE);
			stop_ecal_timer();
		}
		
		// Connect to calendar events
		g_signal_connect(calendar, "event::month-changed", G_CALLBACK(month_changed_cb), NULL);
		g_signal_connect(calendar, "event::day-selected", G_CALLBACK(day_selected_cb), NULL);
		g_signal_connect(calendar, "event::day-selected-double-click", G_CALLBACK(day_selected_double_click_cb), NULL);
	} else {
		g_debug("Unable to find calendar app.");
		if (add_appointment != NULL)
			dbusmenu_menuitem_property_set_bool(add_appointment, DBUSMENU_MENUITEM_PROP_VISIBLE, FALSE);
		if (events_separator != NULL)
			dbusmenu_menuitem_property_set_bool(events_separator, DBUSMENU_MENUITEM_PROP_VISIBLE, FALSE);
	}
	
	if (g_settings_get_boolean(conf, SETTINGS_SHOW_CALENDAR_S)) {
		dbusmenu_menuitem_property_set_bool(calendar, DBUSMENU_MENUITEM_PROP_ENABLED, TRUE);
		dbusmenu_menuitem_property_set_bool(calendar, DBUSMENU_MENUITEM_PROP_VISIBLE, TRUE);
	} else {
		dbusmenu_menuitem_property_set_bool(calendar, DBUSMENU_MENUITEM_PROP_ENABLED, FALSE);
		dbusmenu_menuitem_property_set_bool(calendar, DBUSMENU_MENUITEM_PROP_VISIBLE, FALSE);
	}

	return FALSE;
}

// Authentication function
static gchar *
auth_func (ECal *ecal, 
           const gchar *prompt, 
           const gchar *key, 
           gpointer user_data)
{
	ESource *source = e_cal_get_source (ecal);
	gchar *auth_domain = e_source_get_duped_property (source, "auth-domain");

	const gchar *component_name;
	if (auth_domain) component_name = auth_domain;
	else component_name = "Calendar";
	
	gchar *password = e_passwords_get_password (component_name, key);
	
	g_free (auth_domain);

	return password;
}

static gint
compare_comp_instances (gconstpointer ga, gconstpointer gb)
{
	const struct comp_instance * a = ga;
	const struct comp_instance * b = gb;

	/* sort by start time */
	if (a->start < b->start) return -1;
	if (a->start > b->start) return  1;
	return 0;
}

static struct comp_instance*
comp_instance_new (ECalComponent * comp, time_t start, time_t end, ESource * source)
{
	g_debug("Using times start %s, end %s", ctime(&start), ctime(&end));

	struct comp_instance *ci = g_new (struct comp_instance, 1);
	ci->comp = g_object_ref (comp);
	ci->source = source;
	ci->start = start;
	ci->end = end;
	return ci;
}
static void
comp_instance_free (struct comp_instance* ci)
{
	if (ci != NULL) {
		g_clear_object (&ci->comp);
		g_free (ci);
	}
}

static gboolean
populate_appointment_instances (ECalComponent * comp,
                                time_t          start,
                                time_t          end,
                                gpointer        data)
{
	g_debug("Appending item %p", comp);
	
	ECalComponentVType vtype = e_cal_component_get_vtype (comp);
	if (vtype != E_CAL_COMPONENT_EVENT && vtype != E_CAL_COMPONENT_TODO) return FALSE;
	
	icalproperty_status status;
	e_cal_component_get_status (comp, &status);
	if (status == ICAL_STATUS_COMPLETED || status == ICAL_STATUS_CANCELLED) return FALSE;

	struct comp_instance *ci = comp_instance_new (comp, start, end, E_SOURCE(data));
	comp_instances = g_list_append (comp_instances, ci);
	return TRUE;
}

/* Populate the menu with todays, next 5 appointments. 
 * we should hook into the ABOUT TO SHOW signal and use that to update the appointments.
 * Experience has shown that caldav's and webcals can be slow to load from eds
 * this is a problem mainly on the EDS side of things, not ours. 
 */
static gboolean
update_appointment_menu_items (gpointer user_data)
{
	// FFR: we should take into account short term timers, for instance
	// tea timers, pomodoro timers etc... that people may add, this is hinted to in the spec.
	g_debug("Update appointments called");
	if (calendar == NULL) return FALSE;
	if (!g_settings_get_boolean(conf, SETTINGS_SHOW_EVENTS_S)) return FALSE;
	if (updating_appointments) return TRUE;
	updating_appointments = TRUE;
	
	time_t curtime = 0, t1 = 0, t2 = 0;
	GList *l;
	GSList *g;
	GError *gerror = NULL;
	gint i;
	gint width = 0, height = 0;
	ESourceList * sources = NULL;

	// Get today & work out query times
	time(&curtime);
	struct tm *today = localtime(&curtime);
	const int mday = today->tm_mday;
	const int mon = today->tm_mon;
	const int year = today->tm_year;

	int start_month_saved = mon;

  	struct tm *start_tm = NULL;
	int this_year = today->tm_year + 1900;
	int days[12]={31,28,31,30,31,30,31,31,30,31,30,31};
	if ((this_year % 400 == 0) || (this_year % 100 > 0 && this_year % 4 == 0)) days[1] = 29;
	
	int highlightdays = days[mon] - mday + 1;
	t1 = curtime; // By default the current time is the appointment start time. 
	
	if (start_time_appointments > 0) {
  		start_tm = localtime(&start_time_appointments);
		int start_month = start_tm->tm_mon;
		start_month_saved = start_month;
		int start_year = start_tm->tm_year + 1900;
		if ((start_month != mon) || (start_year != this_year)) {
			// Set t1 to the start of that month.
			struct tm month_start = {0};
			month_start.tm_year = start_tm->tm_year;
			month_start.tm_mon = start_tm->tm_mon;
			month_start.tm_mday = 1;
			t1 = mktime(&month_start);
			highlightdays = days[start_month];
		}
	}
	
	g_debug("Will highlight %d days from %s", highlightdays, ctime(&t1));

	highlightdays = highlightdays + 7; // Minimum of 7 days ahead 
	t2 = t1 + (time_t) (highlightdays * 24 * 60 * 60);
	
	if (!e_cal_get_sources(&sources, E_CAL_SOURCE_TYPE_EVENT, &gerror)) {
		g_debug("Failed to get ecal sources\n");
		g_clear_error (&gerror);
		return FALSE;
	}
	
	// clear any previous comp_instances
	g_list_free_full (comp_instances, (GDestroyNotify)comp_instance_free);
	comp_instances = NULL;

	GSList *cal_list = gconf_client_get_list(gconf, "/apps/evolution/calendar/display/selected_calendars", GCONF_VALUE_STRING, &gerror);
	if (gerror) {
	  g_debug("Failed to get evolution preference for enabled calendars");
	  g_clear_error (&gerror);
	  cal_list = NULL;
	}
	
	// Generate instances for all sources
	for (g = e_source_list_peek_groups (sources); g; g = g->next) {
		ESourceGroup *group = E_SOURCE_GROUP (g->data);
		GSList *s;
		
		for (s = e_source_group_peek_sources (group); s; s = s->next) {
			ESource *source = E_SOURCE (s->data);
			g_signal_connect (G_OBJECT(source), "changed", G_CALLBACK (update_appointment_menu_items), NULL);
			ECal *ecal = e_cal_new(source, E_CAL_SOURCE_TYPE_EVENT);
			e_cal_set_auth_func (ecal, (ECalAuthFunc) auth_func, NULL);
			
			icaltimezone* current_zone = icaltimezone_get_builtin_timezone(current_timezone);
			if (!current_zone) {
				// current_timezone may be a TZID?
				current_zone = icaltimezone_get_builtin_timezone_from_tzid(current_timezone);
			}
			if (current_zone && !e_cal_set_default_timezone(ecal, current_zone, &gerror)) {
				g_debug("Failed to set ecal default timezone %s", gerror->message);
				g_clear_error (&gerror);
				g_object_unref(ecal);
				continue;
			}
			
			if (!e_cal_open(ecal, FALSE, &gerror)) {
				g_debug("Failed to get ecal sources %s", gerror->message);
				g_clear_error (&gerror);
				g_object_unref(ecal);
				continue;
			}

			const gchar *ecal_uid = e_source_peek_uid(source);
			g_debug("Checking ecal_uid is enabled: %s", ecal_uid);
			const gboolean in_list = g_slist_find_custom (cal_list, ecal_uid, (GCompareFunc)g_strcmp0) != NULL;
			if (!in_list) {
				g_object_unref(ecal);
				continue;
			}

			g_debug("ecal_uid is enabled, generating instances");
			e_cal_generate_instances (ecal, t1, t2, (ECalRecurInstanceFn) populate_appointment_instances, source);
			g_object_unref(ecal);
		}
	}
	g_slist_free_full (cal_list, g_free);

	g_debug("Number of ECalComponents returned: %d", g_list_length(comp_instances));
	GList *sorted_comp_instances = g_list_sort(comp_instances, compare_comp_instances);
	comp_instances = NULL;
	g_debug("Components sorted");
	
	/* Hiding all of the previous appointments */
	if (appointments != NULL) {
		g_debug("Hiding old appointments");
		GList * appointment;
		for (appointment = appointments; appointment != NULL; appointment = g_list_next(appointment)) {
			DbusmenuMenuitem * litem =  DBUSMENU_MENUITEM(appointment->data);
			g_debug("Hiding old appointment: %p", litem);
			// Remove all the existing menu items which are in appointments.
			dbusmenu_menuitem_property_set_bool(DBUSMENU_MENUITEM(litem), DBUSMENU_MENUITEM_PROP_VISIBLE, FALSE);
		}
	}

	gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, &width, &height);
	if (width <= 0) width = 12;
	if (height <= 0) height = 12;
	if (width > 30) width = 12;
	if (height > 30) height = 12;
	
	gchar *time_format_str = g_settings_get_string(conf, SETTINGS_TIME_FORMAT_S);
	gint apt_output;
	if (g_strcmp0(time_format_str, "12-hour") == 0) {
		apt_output = SETTINGS_TIME_12_HOUR;
	} else if (g_strcmp0(time_format_str, "24-hour") == 0) {
		apt_output = SETTINGS_TIME_24_HOUR;
	} else if (is_locale_12h()) {
		apt_output = SETTINGS_TIME_12_HOUR;
	} else {
		apt_output = SETTINGS_TIME_24_HOUR;
	}
	g_free (time_format_str);
	
	GVariantBuilder markeddays;
	g_variant_builder_init (&markeddays, G_VARIANT_TYPE ("ai"));
	
	i = 0;
	GList * cached_appointment = appointments;
	for (l = sorted_comp_instances; l; l = l->next) {
		struct comp_instance *ci = l->data;
		ECalComponent *ecalcomp = ci->comp;
		char right[20];
		//const gchar *uri;
		DbusmenuMenuitem * item;
		
		ECalComponentVType vtype = e_cal_component_get_vtype (ecalcomp);
		struct tm due_data = {0};
		struct tm *due = NULL;
		if (vtype == E_CAL_COMPONENT_EVENT) due = localtime_r(&ci->start, &due_data);
		else if (vtype == E_CAL_COMPONENT_TODO) due = localtime_r(&ci->end, &due_data);
		else continue;
		
		const int dmday = due->tm_mday;
		const int dmon = due->tm_mon;
		const int dyear = due->tm_year;
		
		if (start_month_saved == dmon) {
			// Mark day if our query hasn't hit the next month. 
			g_debug("Adding marked date %s, %d", ctime(&ci->start), dmday);
			g_variant_builder_add (&markeddays, "i", dmday);
		}
		
		// If the appointment time is less than the selected date, 
		// don't create an appointment item for it.
		if (vtype == E_CAL_COMPONENT_EVENT) {
			if (ci->start < start_time_appointments) continue;
		} else if (vtype == E_CAL_COMPONENT_TODO) {
			if (ci->end < start_time_appointments) continue;
		}
		
		if (i >= 5) continue;
		i++;

		if (cached_appointment == NULL) {
			g_debug("Create menu item");
			
			item = dbusmenu_menuitem_new();
			dbusmenu_menuitem_property_set       (item, DBUSMENU_MENUITEM_PROP_TYPE, APPOINTMENT_MENUITEM_TYPE);

			dbusmenu_menuitem_child_add_position (root, item, 2+i);
			appointments = g_list_append (appointments, item); // Keep track of the items here to make them easy to remove
		} else {
			item = DBUSMENU_MENUITEM(cached_appointment->data);
			cached_appointment = g_list_next(cached_appointment);

			/* Remove the icon as we might not replace it on error */
			dbusmenu_menuitem_property_remove(item, APPOINTMENT_MENUITEM_PROP_ICON);

			/* Remove the activate handler */
			g_signal_handlers_disconnect_matched(G_OBJECT(item), G_SIGNAL_MATCH_FUNC, 0, 0, NULL, G_CALLBACK(activate_cb), NULL);
		}

		dbusmenu_menuitem_property_set_bool  (item, DBUSMENU_MENUITEM_PROP_ENABLED, TRUE);
		dbusmenu_menuitem_property_set_bool  (item, DBUSMENU_MENUITEM_PROP_VISIBLE, TRUE);

	
        // Label text        
		ECalComponentText valuetext;
		e_cal_component_get_summary (ecalcomp, &valuetext);
		const gchar * summary = valuetext.value;
		g_debug("Summary: %s", summary);
		dbusmenu_menuitem_property_set (item, APPOINTMENT_MENUITEM_PROP_LABEL, summary);

		gboolean full_day = FALSE;
		if (vtype == E_CAL_COMPONENT_EVENT) {
			time_t start = ci->start;
			if (time_add_day(start, 1) == ci->end) {
				full_day = TRUE;
			}
		}

		// Due text
		if (full_day) {
			struct tm fulldaytime = {0};
			gmtime_r(&ci->start, &fulldaytime);

			/* TRANSLATORS: This is a strftime string for the day for full day events
			   in the menu.  It should most likely be either '%A' for a full text day
			   (Wednesday) or '%a' for a shortened one (Wed).  You should only need to
			   change for '%a' in the case of langauges with very long day names. */
			strftime(right, 20, _("%A"), &fulldaytime);
		} else {
			if (apt_output == SETTINGS_TIME_12_HOUR) {
				if ((mday == dmday) && (mon == dmon) && (year == dyear))
					strftime(right, 20, _(DEFAULT_TIME_12_FORMAT), due);
				else
					strftime(right, 20, _(DEFAULT_TIME_12_FORMAT_WITH_DAY), due);
			} else if (apt_output == SETTINGS_TIME_24_HOUR) {
				if ((mday == dmday) && (mon == dmon) && (year == dyear))
					strftime(right, 20, _(DEFAULT_TIME_24_FORMAT), due);
				else
					strftime(right, 20, _(DEFAULT_TIME_24_FORMAT_WITH_DAY), due);
			}
		}
		g_debug("Appointment time: %s, for date %s", right, asctime(due));
		dbusmenu_menuitem_property_set (item, APPOINTMENT_MENUITEM_PROP_RIGHT, right);
		
		// Now we pull out the URI for the calendar event and try to create a URI that'll work when we execute evolution
		// FIXME Because the URI stuff is really broken, we're going to open the calendar at todays date instead
		//e_cal_component_get_uid(ecalcomp, &uri);
		gchar * ad = isodate_from_time_t(mktime(due));
		gchar * cmd = g_strconcat("evolution calendar:///?startdate=", ad, NULL);
		g_debug("Command to Execute: %s", cmd);
		g_signal_connect_data (G_OBJECT(item), DBUSMENU_MENUITEM_SIGNAL_ITEM_ACTIVATED,
		                       G_CALLBACK(activate_cb), cmd, (GClosureNotify)g_free, 0);
		g_free (ad);

        const gchar *color_spec = e_source_peek_color_spec(ci->source);
        g_debug("Colour to use: %s", color_spec);
			
		// Draw the correct icon for the appointment type and then tint it using mask fill.
		// For now we'll create a circle
        if (color_spec != NULL) {
        	g_debug("Creating a cairo surface: size, %d by %d", width, height);         
        	cairo_surface_t *surface = cairo_image_surface_create( CAIRO_FORMAT_ARGB32, width, height ); 
			cairo_t *cr = cairo_create(surface);
        	GdkRGBA rgba;
        	if (gdk_rgba_parse (&rgba, color_spec))
        		gdk_cairo_set_source_rgba (cr, &rgba);
			cairo_paint(cr);
    		cairo_set_source_rgba(cr, 0,0,0,0.5);
    		cairo_set_line_width(cr, 1);
    		cairo_rectangle (cr, 0.5, 0.5, width-1, height-1);
    		cairo_stroke(cr);
			// Convert to pixbuf, in gtk3 this is done with gdk_pixbuf_get_from_surface
			cairo_content_t content = cairo_surface_get_content (surface) | CAIRO_CONTENT_COLOR;
			GdkPixbuf *pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, 
			                                    !!(content & CAIRO_CONTENT_ALPHA), 
			                                    8, width, height);
			if (pixbuf != NULL) {               
				gint sstride = cairo_image_surface_get_stride( surface ); 
				gint dstride = gdk_pixbuf_get_rowstride (pixbuf);
				guchar *spixels = cairo_image_surface_get_data( surface );
				guchar *dpixels = gdk_pixbuf_get_pixels (pixbuf);

	  			int x, y;
	  			for (y = 0; y < height; y++) {
					guint32 *src = (guint32 *) spixels;

					for (x = 0; x < width; x++) {
						guint alpha = src[x] >> 24;

						if (alpha == 0) {
		      				dpixels[x * 4 + 0] = 0;
		      				dpixels[x * 4 + 1] = 0;
		      				dpixels[x * 4 + 2] = 0;
		    			} else {
							dpixels[x * 4 + 0] = (((src[x] & 0xff0000) >> 16) * 255 + alpha / 2) / alpha;
							dpixels[x * 4 + 1] = (((src[x] & 0x00ff00) >>  8) * 255 + alpha / 2) / alpha;
							dpixels[x * 4 + 2] = (((src[x] & 0x0000ff) >>  0) * 255 + alpha / 2) / alpha;
						}
						dpixels[x * 4 + 3] = alpha;
					}
					spixels += sstride;
					dpixels += dstride;
	  			}
	  			
				dbusmenu_menuitem_property_set_image (item, APPOINTMENT_MENUITEM_PROP_ICON, pixbuf);
				g_clear_object (&pixbuf);
			} else {
				g_debug("Creating pixbuf from surface failed");
			}
			cairo_surface_destroy (surface);
			cairo_destroy(cr);
		}
		g_debug("Adding appointment: %p", item);
	}
	
	g_clear_error (&gerror);

	g_list_free_full (sorted_comp_instances, (GDestroyNotify)comp_instance_free);
	sorted_comp_instances = NULL;
	
	GVariant * marks = g_variant_builder_end (&markeddays);
	dbusmenu_menuitem_property_set_variant (calendar, CALENDAR_MENUITEM_PROP_MARKS, marks);

	g_clear_object (&sources);
	
	updating_appointments = FALSE;
	g_debug("End of objects");
	return TRUE;
}

/* Looks for the time and date admin application and enables the
   item we have one */
static gboolean
check_for_timeadmin (gpointer user_data)
{
	g_return_val_if_fail (settings != NULL, FALSE);

	gchar * timeadmin = g_find_program_in_path("gnome-control-center");
	if (timeadmin != NULL) {
		g_debug("Found the gnome-control-center application: %s", timeadmin);
		dbusmenu_menuitem_property_set_bool(settings, DBUSMENU_MENUITEM_PROP_ENABLED, TRUE);
		g_free(timeadmin);
	} else {
		g_debug("Unable to find gnome-control-center app.");
		dbusmenu_menuitem_property_set_bool(settings, DBUSMENU_MENUITEM_PROP_ENABLED, FALSE);
	}

	return FALSE;
}

static void
show_locations_changed (void)
{
	/* Re-calculate */
	update_location_menu_items();
}

static void
time_format_changed (void)
{
	update_appointment_menu_items(NULL);
}

/* Does the work to build the default menu, really calls out
   to other functions but this is the core to clean up the
   main function. */
static void
build_menus (DbusmenuMenuitem * root)
{
	g_debug("Building Menus.");
	if (date == NULL) {
		date = dbusmenu_menuitem_new();
		dbusmenu_menuitem_property_set     (date, DBUSMENU_MENUITEM_PROP_LABEL, _("No date yet…"));
		dbusmenu_menuitem_property_set_bool(date, DBUSMENU_MENUITEM_PROP_ENABLED, FALSE);
		dbusmenu_menuitem_child_append(root, date);

		g_idle_add(update_datetime, NULL);
	}

	if (calendar == NULL) {
		calendar = dbusmenu_menuitem_new();
		dbusmenu_menuitem_property_set (calendar, DBUSMENU_MENUITEM_PROP_TYPE, DBUSMENU_CALENDAR_MENUITEM_TYPE);
		/* insensitive until we check for available apps */
		dbusmenu_menuitem_property_set_bool(calendar, DBUSMENU_MENUITEM_PROP_ENABLED, FALSE);
		g_signal_connect (G_OBJECT(calendar), DBUSMENU_MENUITEM_SIGNAL_ITEM_ACTIVATED,
						  G_CALLBACK (activate_cb), "evolution -c calendar");
		dbusmenu_menuitem_child_append(root, calendar);

		g_idle_add(check_for_calendar, NULL);
	}

	if (!get_greeter_mode ()) {
		locations_separator = dbusmenu_menuitem_new();
		dbusmenu_menuitem_property_set(locations_separator, DBUSMENU_MENUITEM_PROP_TYPE, DBUSMENU_CLIENT_TYPES_SEPARATOR);
		dbusmenu_menuitem_property_set_bool (locations_separator, DBUSMENU_MENUITEM_PROP_VISIBLE, FALSE);
		dbusmenu_menuitem_child_append(root, locations_separator);

		update_location_menu_items();
	
		g_signal_connect (conf, "changed::" SETTINGS_SHOW_LOCATIONS_S, G_CALLBACK (show_locations_changed), NULL);
		g_signal_connect (conf, "changed::" SETTINGS_SHOW_DETECTED_S, G_CALLBACK (show_locations_changed), NULL);
		g_signal_connect (conf, "changed::" SETTINGS_LOCATIONS_S, G_CALLBACK (show_locations_changed), NULL);
		g_signal_connect (conf, "changed::" SETTINGS_SHOW_EVENTS_S, G_CALLBACK (show_events_changed), NULL);
		g_signal_connect (conf, "changed::" SETTINGS_TIME_FORMAT_S, G_CALLBACK (time_format_changed), NULL);

		DbusmenuMenuitem * separator = dbusmenu_menuitem_new();
		dbusmenu_menuitem_property_set(separator, DBUSMENU_MENUITEM_PROP_TYPE, DBUSMENU_CLIENT_TYPES_SEPARATOR);
		dbusmenu_menuitem_child_append(root, separator);

		settings = dbusmenu_menuitem_new();
		dbusmenu_menuitem_property_set     (settings, DBUSMENU_MENUITEM_PROP_LABEL, _("Time & Date Settings…"));
		/* insensitive until we check for available apps */
		dbusmenu_menuitem_property_set_bool(settings, DBUSMENU_MENUITEM_PROP_ENABLED, FALSE);
		g_signal_connect(G_OBJECT(settings), DBUSMENU_MENUITEM_SIGNAL_ITEM_ACTIVATED, G_CALLBACK(activate_cb), SETTINGS_APP_INVOCATION);
		dbusmenu_menuitem_child_append(root, settings);
		g_idle_add(check_for_timeadmin, NULL);
	}

	return;
}

/* Run when the timezone file changes */
static void
timezone_changed (GFileMonitor * monitor, GFile * file, GFile * otherfile, GFileMonitorEvent event, gpointer user_data)
{
	update_current_timezone();
	datetime_interface_update(DATETIME_INTERFACE(user_data));
	update_datetime(NULL);
	setup_timer();
	return;
}

/* Set up monitoring the timezone file */
static void
build_timezone (DatetimeInterface * dbus)
{
	GFile * timezonefile = g_file_new_for_path(TIMEZONE_FILE);
	GFileMonitor * monitor = g_file_monitor_file(timezonefile, G_FILE_MONITOR_NONE, NULL, NULL);
	if (monitor != NULL) {
		g_signal_connect(G_OBJECT(monitor), "changed", G_CALLBACK(timezone_changed), dbus);
		g_debug("Monitoring timezone file: '" TIMEZONE_FILE "'");
	} else {
		g_warning("Unable to monitor timezone file: '" TIMEZONE_FILE "'");
	}
	return;
}

/* Source ID for the timer */
static guint timer = 0;

/* Execute at a given time, update and setup a new
   timer to go again.  */
static gboolean
timer_func (gpointer user_data)
{
	timer = 0;
	/* Reset up each time to reduce error */
	setup_timer();
	update_datetime(NULL);
	return FALSE;
}

/* Sets up the time to launch the timer to update the
   date in the datetime entry */
static void
setup_timer (void)
{
	if (timer != 0) {
		g_source_remove(timer);
		timer = 0;
	}

	time_t t;
	t = time(NULL);
	struct tm * ltime = localtime(&t);

	timer = g_timeout_add_seconds(((23 - ltime->tm_hour) * 60 * 60) +
	                              ((59 - ltime->tm_min) * 60) +
	                              ((60 - ltime->tm_sec)) + 60 /* one minute past */,
	                              timer_func, NULL);

	return;
}

static void
session_active_change_cb (GDBusProxy * proxy, gchar * sender_name, gchar * signal_name,
                          GVariant * parameters, gpointer user_data)
{
	// Just returned from suspend
	if (g_strcmp0(signal_name, "SystemIdleHintChanged") == 0) {
		gboolean idle = FALSE;
		g_variant_get(parameters, "(b)", &idle);
		if (!idle) {
			datetime_interface_update(DATETIME_INTERFACE(user_data));
			update_datetime(NULL);
			setup_timer();
		}
	}
	return;
}

/* for hooking into console kit signal on wake from suspend */
static void
system_proxy_cb (GObject * object, GAsyncResult * res, gpointer user_data)
{
	GError * error = NULL;
	
	GDBusProxy * proxy = g_dbus_proxy_new_for_bus_finish(res, &error);

	if (error != NULL) {
		g_warning("Could not grab DBus proxy for ConsoleKit: %s", error->message);
		g_clear_error (&error);
		return;
	}

	g_signal_connect(proxy, "g-signal", G_CALLBACK(session_active_change_cb), user_data);
}

/* Callback from getting the address */
static void
geo_address_cb (GeoclueAddress * address, int timestamp, GHashTable * addy_data, GeoclueAccuracy * accuracy, GError * error, gpointer user_data)
{
	if (error != NULL) {
		g_warning("Unable to get Geoclue address: %s", error->message);
		g_clear_error (&error);
		return;
	}

	g_debug("Geoclue timezone is: %s", (gchar *)g_hash_table_lookup(addy_data, "timezone"));

	if (geo_timezone != NULL) {
		g_free(geo_timezone);
		geo_timezone = NULL;
	}

	gpointer tz_hash = g_hash_table_lookup(addy_data, "timezone");
	if (tz_hash != NULL) {
		geo_timezone = g_strdup((gchar *)tz_hash);
	}

	update_location_menu_items();

	return;
}

/* Clean up the reference we kept to the address and make sure to
   drop the signals incase someone else has one. */
static void
geo_address_clean (void)
{
	if (geo_address == NULL) {
		return;
	}

	g_signal_handlers_disconnect_by_func(G_OBJECT(geo_address), geo_address_cb, NULL);
	g_object_unref(G_OBJECT(geo_address));

	geo_address = NULL;

	return;
}

/* Clean up and remove all signal handlers from the client as we
   unreference it as well. */
static void
geo_client_clean (void)
{
	if (geo_master == NULL) {
		return;
	}

	g_signal_handlers_disconnect_by_func(G_OBJECT(geo_master), geo_client_invalid, NULL);
	g_signal_handlers_disconnect_by_func(G_OBJECT(geo_master), geo_address_change, NULL);
	g_object_unref(G_OBJECT(geo_master));

	geo_master = NULL;

	return;
}

/* Callback from creating the address */
static void
geo_create_address (GeoclueMasterClient * master, GeoclueAddress * address, GError * error, gpointer user_data)
{
	if (error != NULL) {
		g_warning("Unable to create GeoClue address: %s", error->message);
		g_clear_error (&error);
		return;
	}

	/* We shouldn't have created a new address if we already had one
	   so this is a warning.  But, it really is only a mem-leak so we
	   don't need to error out. */
	g_warn_if_fail(geo_address == NULL);
	geo_address_clean();

	g_debug("Created Geoclue Address");
	geo_address = address;
	g_object_ref(G_OBJECT(geo_address));

	geoclue_address_get_address_async(geo_address, geo_address_cb, NULL);

	g_signal_connect(G_OBJECT(address), "address-changed", G_CALLBACK(geo_address_cb), NULL);

	return;
}

/* Callback from setting requirements */
static void
geo_req_set (GeoclueMasterClient * master, GError * error, gpointer user_data)
{
	if (error != NULL) {
		g_warning("Unable to set Geoclue requirements: %s", error->message);
		g_clear_error (&error);
	}
	return;
}

/* Client is killing itself rather oddly */
static void
geo_client_invalid (GeoclueMasterClient * client, gpointer user_data)
{
	g_warning("Master client invalid, rebuilding.");

	/* Client changes we can assume the address is now invalid so we
	   need to unreference the one we had. */
	geo_address_clean();

	/* And our master client is invalid */
	geo_client_clean();

	GeoclueMaster * master = geoclue_master_get_default();
	geoclue_master_create_client_async(master, geo_create_client, NULL);

	if (geo_timezone != NULL) {
		g_free(geo_timezone);
		geo_timezone = NULL;
	}

	update_location_menu_items();

	return;
}

/* Address provider changed, we need to get that one */
static void
geo_address_change (GeoclueMasterClient * client, gchar * a, gchar * b, gchar * c, gchar * d, gpointer user_data)
{
	g_warning("Address provider changed.  Let's change");

	/* If the address is supposed to have changed we need to drop the old
	   address before starting to get the new one. */
	geo_address_clean();

	geoclue_master_client_create_address_async(geo_master, geo_create_address, NULL);

	if (geo_timezone != NULL) {
		g_free(geo_timezone);
		geo_timezone = NULL;
	}

	update_location_menu_items();

	return;
}

/* Callback from creating the client */
static void
geo_create_client (GeoclueMaster * master, GeoclueMasterClient * client, gchar * path, GError * error, gpointer user_data)
{
	g_debug("Created Geoclue client at: %s", path);

	geo_master = client;

	if (error != NULL) {
		g_warning("Unable to get a GeoClue client!  '%s'  Geolocation based timezone support will not be available.", error->message);
		g_clear_error (&error);
		return;
	}

	if (geo_master == NULL) {
		g_warning(_("Unable to get a GeoClue client!  Geolocation based timezone support will not be available."));
		return;
	}

	g_object_ref(G_OBJECT(geo_master));

	/* New client, make sure we don't have an address hanging on */
	geo_address_clean();

	geoclue_master_client_set_requirements_async(geo_master,
	                                             GEOCLUE_ACCURACY_LEVEL_REGION,
	                                             0,
	                                             FALSE,
	                                             GEOCLUE_RESOURCE_ALL,
	                                             geo_req_set,
	                                             NULL);

	geoclue_master_client_create_address_async(geo_master, geo_create_address, NULL);

	g_signal_connect(G_OBJECT(client), "invalidated", G_CALLBACK(geo_client_invalid), NULL);
	g_signal_connect(G_OBJECT(client), "address-provider-changed", G_CALLBACK(geo_address_change), NULL);

	return;
}

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
service_shutdown (IndicatorService * service, gpointer user_data)
{
	g_warning("Shutting down service!");
	g_main_loop_quit(mainloop);
	return;
}

/* Function to build everything up.  Entry point from asm. */
int
main (int argc, char ** argv)
{
	g_type_init();

	/* Acknowledging the service init and setting up the interface */
	service = indicator_service_new_version(SERVICE_NAME, SERVICE_VERSION);
	g_signal_connect(service, INDICATOR_SERVICE_SIGNAL_SHUTDOWN, G_CALLBACK(service_shutdown), NULL);

	/* Setting up i18n and gettext.  Apparently, we need
	   all of these. */
	setlocale (LC_ALL, "");
	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	textdomain (GETTEXT_PACKAGE);

	/* Set up GSettings */
	conf = g_settings_new(SETTINGS_INTERFACE);
	/* Set up gconf for getting evolution enabled calendars */
	gconf = gconf_client_get_default();
	// TODO Add a signal handler to catch gsettings changes and respond to them

	/* Building the base menu */
	server = dbusmenu_server_new(MENU_OBJ);
	root = dbusmenu_menuitem_new();
	dbusmenu_server_set_root(server, root);
	
	build_menus(root);
	
	/* Cache the timezone */
	update_current_timezone();

	/* Setup geoclue */
	GeoclueMaster * master = geoclue_master_get_default();
	geoclue_master_create_client_async(master, geo_create_client, NULL);

	/* Setup dbus interface */
	dbus = g_object_new(DATETIME_INTERFACE_TYPE, NULL);

	/* Setup timezone watch */
	build_timezone(dbus);

	/* Setup the timer */
	setup_timer();

	/* And watch for system resumes */
	g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
		                  G_DBUS_PROXY_FLAGS_NONE,
		                  NULL,
		                  "org.freedesktop.ConsoleKit",
		                  "/org/freedesktop/ConsoleKit/Manager",
		                  "org.freedesktop.ConsoleKit.Manager",
		                  NULL, system_proxy_cb, dbus);

	mainloop = g_main_loop_new(NULL, FALSE);
	g_main_loop_run(mainloop);

	g_object_unref(G_OBJECT(conf));
	g_object_unref(G_OBJECT(master));
	g_object_unref(G_OBJECT(dbus));
	g_object_unref(G_OBJECT(service));
	g_object_unref(G_OBJECT(server));
	g_object_unref(G_OBJECT(root));

	icaltimezone_free_builtin_timezones();

	geo_address_clean();
	geo_client_clean();

	return 0;
}
