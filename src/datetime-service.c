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

#include <glib/gi18n.h>
#include <gio/gio.h>

#include <libdbusmenu-glib/server.h>
#include <libdbusmenu-glib/client.h>
#include <libdbusmenu-glib/menuitem.h>

#include <geoclue/geoclue-master.h>
#include <geoclue/geoclue-master-client.h>

#include <time.h>
#include <libecal/e-cal.h>
#include <libical/ical.h>
// Other users of ecal seem to also include these, not sure why they should be included by the above
#include <libecal/e-cal-time-util.h>
#include <libical/icaltime.h>

#include <oobs/oobs-timeconfig.h>

#include "datetime-interface.h"
#include "dbus-shared.h"

static void geo_create_client (GeoclueMaster * master, GeoclueMasterClient * client, gchar * path, GError * error, gpointer user_data);
static void setup_timer (void);

static IndicatorService * service = NULL;
static GMainLoop * mainloop = NULL;
static DbusmenuServer * server = NULL;
static DbusmenuMenuitem * root = NULL;
static DatetimeInterface * dbus = NULL;
static gchar * current_timezone = NULL;

/* Global Items */
static DbusmenuMenuitem * date = NULL;
static DbusmenuMenuitem * calendar = NULL;
static DbusmenuMenuitem * settings = NULL;
static DbusmenuMenuitem * tzchange = NULL;
static GList			* appointments = NULL;
static ECal				* ecal = NULL;
static gchar			* ecal_timezone = NULL;

/* Geoclue trackers */
static GeoclueMasterClient * geo_master = NULL;
static GeoclueAddress * geo_address = NULL;
static gchar * geo_timezone = NULL;

/* Check to see if our timezones are the same */
static void
check_timezone_sync (void) {
	gboolean in_sync = FALSE;

	if (geo_timezone == NULL) {
		in_sync = TRUE;
	}

	if (current_timezone == NULL) {
		in_sync = TRUE;
	}

	if (!in_sync && g_strcmp0(geo_timezone, current_timezone) == 0) {
		in_sync = TRUE;
	}

	if (in_sync) {
		g_debug("Timezones in sync");
	} else {
		g_debug("Timezones are different");
	}

	if (tzchange != NULL) {
		if (in_sync) {
			dbusmenu_menuitem_property_set_bool(tzchange, DBUSMENU_MENUITEM_PROP_VISIBLE, FALSE);
		} else {
			gchar * label = g_strdup_printf(_("Change timezone to: %s"), geo_timezone);

			dbusmenu_menuitem_property_set(tzchange, DBUSMENU_MENUITEM_PROP_LABEL, label);
			dbusmenu_menuitem_property_set_bool(tzchange, DBUSMENU_MENUITEM_PROP_VISIBLE, TRUE);

			g_free(label);
		}
	}

	return;
}

/* Update the current timezone */
static void
update_current_timezone (void) {
	/* Clear old data */
	if (current_timezone != NULL) {
		g_free(current_timezone);
		current_timezone = NULL;
	}

	GError * error = NULL;
	gchar * tempzone = NULL;
	if (!g_file_get_contents(TIMEZONE_FILE, &tempzone, NULL, &error)) {
		g_warning("Unable to read timezone file '" TIMEZONE_FILE "': %s", error->message);
		g_error_free(error);
		return;
	}

	/* This shouldn't happen, so let's make it a big boom! */
	g_return_if_fail(tempzone != NULL);

	/* Note: this really makes sense as strstrip works in place
	   so we end up with something a little odd without the dup
	   so we have the dup to make sure everything is as expected
	   for everyone else. */
	current_timezone = g_strdup(g_strstrip(tempzone));
	g_free(tempzone);

	g_debug("System timezone is: %s", current_timezone);

	check_timezone_sync();

	return;
}

/* See how our timezone setting went */
static void
quick_set_tz_cb (OobsObject * obj, OobsResult result, gpointer user_data)
{
	if (result == OOBS_RESULT_OK) {
		g_debug("Timezone set");
	} else {
		g_warning("Unable to quick set timezone");
	}
	return;
}

/* Set the timezone to the Geoclue discovered one */
static void
quick_set_tz (DbusmenuMenuitem * menuitem, guint timestamp, const gchar *command)
{
	g_debug("Quick setting timezone to: %s", geo_timezone);

	g_return_if_fail(geo_timezone != NULL);

	OobsObject * obj = oobs_time_config_get();
	g_return_if_fail(obj != NULL);

	OobsTimeConfig * timeconfig = OOBS_TIME_CONFIG(obj);
	oobs_time_config_set_timezone(timeconfig, geo_timezone);

	oobs_object_commit_async(obj, quick_set_tz_cb, NULL);

	return;
}

/* Updates the label in the date menuitem */
static gboolean
update_datetime (gpointer user_data)
{
	g_debug("Updating Date/Time");

	gchar longstr[128];
	time_t t;
	struct tm *ltime;

	t = time(NULL);
	ltime = localtime(&t);
	if (ltime == NULL) {
		g_warning("Error getting local time");
		dbusmenu_menuitem_property_set(date, DBUSMENU_MENUITEM_PROP_LABEL, _("Error getting time"));
		return FALSE;
	}

	/* Note: may require some localization tweaks */
	strftime(longstr, 128, "%A, %e %B %Y", ltime);
	
	gchar * utf8 = g_locale_to_utf8(longstr, -1, NULL, NULL, NULL);
	dbusmenu_menuitem_property_set(date, DBUSMENU_MENUITEM_PROP_LABEL, utf8);
	g_free(utf8);

	return FALSE;
}

/* Run a particular program based on an activation */
static void
activate_cb (DbusmenuMenuitem * menuitem, guint timestamp, const gchar *command)
{
	GError * error = NULL;

	g_debug("Issuing command '%s'", command);
	if (!g_spawn_command_line_async(command, &error)) {
		g_warning("Unable to start %s: %s", (char *)command, error->message);
		g_error_free(error);
	}
}

/* Looks for the calendar application and enables the item if
   we have one */
static gboolean
check_for_calendar (gpointer user_data)
{
	g_return_val_if_fail (calendar != NULL, FALSE);

	gchar *evo = g_find_program_in_path("evolution");
	if (evo != NULL) {
		g_debug("Found the calendar application: %s", evo);
		dbusmenu_menuitem_property_set_bool(calendar, DBUSMENU_MENUITEM_PROP_ENABLED, TRUE);
		dbusmenu_menuitem_property_set_bool(calendar, DBUSMENU_MENUITEM_PROP_VISIBLE, TRUE);
		g_free(evo);
	} else {
		g_debug("Unable to find calendar app.");
		dbusmenu_menuitem_property_set_bool(calendar, DBUSMENU_MENUITEM_PROP_VISIBLE, FALSE);
	}

	return FALSE;
}

/* Populate the menu with todays, next 5 appointments. We probably want to shift this off to an idle-add
 * the problem is the inserting order of items and updating them might be difficult. 
 * we should hook into the ABOUT TO SHOW signal and use that to update the appointments. 
 */
static gboolean
update_appointment_menu_items (gpointer user_data) {
	
	time_t t1, t2;
	gchar *query, *is, *ie;
	GList *objects = NULL, *l;
	GError *gerror = NULL;
	DbusmenuMenuitem * item = NULL;
	gint i;
	
	ecal = e_cal_new_system_calendar();
	if (!ecal) {
		g_debug("e_cal_new_system_calendar failed");
		ecal = NULL;
		return FALSE;
	}
	if (!e_cal_open(ecal, FALSE, &gerror) ) {
		g_debug("e_cal_open: %s\n", gerror->message);
		g_free(ecal);
		ecal = NULL;
		return FALSE;
	}
	
	if (!e_cal_get_timezone(ecal, "UTC", &tzone, &gerror) {
		g_debug("failed to get time zone\n");
		g_free(ecal);
		ecal = NULL;
		return FALSE;
	}
	
	ecal_timezone = icaltimezone_get_tzid(tzone);
	
	// TODO: Remove all the existing menu items which are appointments.
	
	time(&t1);
	time(&t2);
	t2 += (time_t) (24 * 60 * 60); /* 1 day ahead of now */

	is = isodate_from_time_t(t1);
	ie = isodate_from_time_t(t2);
	// FIXME can we put a limit on the number of results?
	query = g_strdup_printf("(occur-in-time-range? (make-time\"%s\") (make-time\"%s\"))", is, ie);

	if (!e_cal_get_object_list(ecal, query, &objects, &gerror);) {
		g_debug("Failed to get objects\n");
		g_free(ecal);
		ecal = NULL;
		return FALSE;
	}
	i = 0;
	gint width, height;
	gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, &width, &height);
	
	for (l = objects; l; l = l->next) {
		icalcomponent *icalcomp = l->data;
		icalproperty* p;
		icalvalue *v;
		gchar *name, *due, *uri;
		p = icalcomponent_get_first_property(icalcomp, ICAL_NAME_PROPERTY);
		v = icalproperty_get_value(p);
		name = icalvalue_get_string(v);
		
		p = icalcomponent_get_first_property(icalcomp, ICAL_DUE_PROPERTY);
		v = icalproperty_get_value(p);
		due = icalvalue_get_string(v);
		
		// TODO: now we pull out the URI for the calendar event and try to create a URI that'll work when we execute evolution
		
		//cairo_surface_t *cs = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
		//cairo_t *cr = cairo_create(cs);

		// TODO: Draw the correct icon for the appointment type and then tint it using mask fill.

		//GdkPixbuf * pixbuf = gdk_pixbuf_get_from_drawable(NULL, (GdkDrawable*)cs, 0,0,0,0, width, height);
		
		// TODO: Create a menu item for each of them, try to include helpful metadata e.g. colours, due time
		item = dbusmenu_menuitem_new();
		dbusmenu_menuitem_property_set       (item, "type", INDICATOR_MENUITEM_TYPE);
		dbusmenu_menuitem_property_set       (item, APPOINTMENT_MENUITEM_PROP_LABEL, name);
		//dbusmenu_menuitem_property_set_image (item, APPOINTMENT_MENUITEM_PROP_ICON, pixbuf);
		dbusmenu_menuitem_property_set       (item, APPOINTMENT_MENUITEM_PROP_RIGHT, due);
		dbusmenu_menuitem_property_set_bool  (item, DBUSMENU_MENUITEM_PROP_ENABLED, TRUE);
		dbusmenu_menuitem_property_set_bool  (item, DBUSMENU_MENUITEM_PROP_VISIBLE, TRUE);
		dbusmenu_menuitem_child_append       (root, item);
		//g_signal_connect (G_OBJECT(item), DBUSMENU_MENUITEM_SIGNAL_ITEM_ACTIVATED,
		//				  G_CALLBACK (activate_cb), "evolution %s", uri);
		
		if (i == 4) break; // See above FIXME regarding query result limit
		i++;
	}
	g_free(ecal); // Really we should do the setup somewhere where we know it'll only run once, right now, we'll do it every time and free it.
	return TRUE;
}

/* Looks for the time and date admin application and enables the
   item we have one */
static gboolean
check_for_timeadmin (gpointer user_data)
{
	g_return_val_if_fail (settings != NULL, FALSE);

	gchar * timeadmin = g_find_program_in_path("time-admin");
	if (timeadmin != NULL) {
		g_debug("Found the time-admin application: %s", timeadmin);
		dbusmenu_menuitem_property_set_bool(settings, DBUSMENU_MENUITEM_PROP_ENABLED, TRUE);
		g_free(timeadmin);
	} else {
		g_debug("Unable to find time-admin app.");
		dbusmenu_menuitem_property_set_bool(settings, DBUSMENU_MENUITEM_PROP_ENABLED, FALSE);
	}

	return FALSE;
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
		dbusmenu_menuitem_property_set     (date, DBUSMENU_MENUITEM_PROP_LABEL, _("No date yet..."));
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
	DbusmenuMenuitem * separator;
	
	separator = dbusmenu_menuitem_new();
	dbusmenu_menuitem_property_set(separator, DBUSMENU_MENUITEM_PROP_TYPE, DBUSMENU_CLIENT_TYPES_SEPARATOR);
	dbusmenu_menuitem_child_append(root, separator);

	// This just populates the items on startup later we want to be able to update the appointments before
	// presenting the menu. 
	update_appointment_menu_items(NULL);

	separator = dbusmenu_menuitem_new();
	dbusmenu_menuitem_property_set(separator, DBUSMENU_MENUITEM_PROP_TYPE, DBUSMENU_CLIENT_TYPES_SEPARATOR);
	dbusmenu_menuitem_child_append(root, separator);

	tzchange = dbusmenu_menuitem_new();
	dbusmenu_menuitem_property_set(tzchange, DBUSMENU_MENUITEM_PROP_LABEL, "Set specific timezone");
	dbusmenu_menuitem_property_set_bool(tzchange, DBUSMENU_MENUITEM_PROP_VISIBLE, FALSE);
	g_signal_connect(G_OBJECT(tzchange), DBUSMENU_MENUITEM_SIGNAL_ITEM_ACTIVATED, G_CALLBACK(quick_set_tz), NULL);
	dbusmenu_menuitem_child_append(root, tzchange);
	check_timezone_sync();

	settings = dbusmenu_menuitem_new();
	dbusmenu_menuitem_property_set     (settings, DBUSMENU_MENUITEM_PROP_LABEL, _("Time & Date Settings..."));
	/* insensitive until we check for available apps */
	dbusmenu_menuitem_property_set_bool(settings, DBUSMENU_MENUITEM_PROP_ENABLED, FALSE);
	g_signal_connect(G_OBJECT(settings), DBUSMENU_MENUITEM_SIGNAL_ITEM_ACTIVATED, G_CALLBACK(activate_cb), "time-admin");
	dbusmenu_menuitem_child_append(root, settings);
	g_idle_add(check_for_timeadmin, NULL);

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

/* Callback from getting the address */
static void
geo_address_cb (GeoclueAddress * address, int timestamp, GHashTable * addy_data, GeoclueAccuracy * accuracy, GError * error, gpointer user_data)
{
	if (error != NULL) {
		g_warning("Unable to get Geoclue address: %s", error->message);
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

	check_timezone_sync();

	return;
}

/* Callback from creating the address */
static void
geo_create_address (GeoclueMasterClient * master, GeoclueAddress * address, GError * error, gpointer user_data)
{
	if (error != NULL) {
		g_warning("Unable to create GeoClue address: %s", error->message);
		return;
	}

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
	}
	return;
}

/* Client is killing itself rather oddly */
static void
geo_client_invalid (GeoclueMasterClient * client, gpointer user_data)
{
	g_warning("Master client invalid, rebuilding.");

	if (geo_master != NULL) {
		g_object_unref(G_OBJECT(geo_master));
	}
	geo_master = NULL;

	GeoclueMaster * master = geoclue_master_get_default();
	geoclue_master_create_client_async(master, geo_create_client, NULL);

	if (geo_timezone != NULL) {
		g_free(geo_timezone);
		geo_timezone = NULL;
	}

	check_timezone_sync();

	return;
}

/* Address provider changed, we need to get that one */
static void
geo_address_change (GeoclueMasterClient * client, gchar * a, gchar * b, gchar * c, gchar * d, gpointer user_data)
{
	g_warning("Address provider changed.  Let's change");

	if (geo_address != NULL) {
		g_object_unref(G_OBJECT(geo_address));
	}
	geo_address = NULL;

	geoclue_master_client_create_address_async(geo_master, geo_create_address, NULL);

	if (geo_timezone != NULL) {
		g_free(geo_timezone);
		geo_timezone = NULL;
	}

	check_timezone_sync();

	return;
}

/* Callback from creating the client */
static void
geo_create_client (GeoclueMaster * master, GeoclueMasterClient * client, gchar * path, GError * error, gpointer user_data)
{
	g_debug("Created Geoclue client at: %s", path);

	geo_master = client;
	g_object_ref(G_OBJECT(geo_master));

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

	/* Cache the timezone */
	update_current_timezone();

	/* Building the base menu */
	server = dbusmenu_server_new(MENU_OBJ);
	root = dbusmenu_menuitem_new();
	dbusmenu_server_set_root(server, root);
	build_menus(root);

	/* Setup geoclue */
	GeoclueMaster * master = geoclue_master_get_default();
	geoclue_master_create_client_async(master, geo_create_client, NULL);

	/* Setup dbus interface */
	dbus = g_object_new(DATETIME_INTERFACE_TYPE, NULL);

	/* Setup timezone watch */
	build_timezone(dbus);

	/* Setup the timer */
	setup_timer();

	mainloop = g_main_loop_new(NULL, FALSE);
	g_main_loop_run(mainloop);

	g_object_unref(G_OBJECT(master));
	g_object_unref(G_OBJECT(dbus));
	g_object_unref(G_OBJECT(service));
	g_object_unref(G_OBJECT(server));
	g_object_unref(G_OBJECT(root));

	return 0;
}
