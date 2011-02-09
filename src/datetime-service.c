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
// Other users of ecal seem to also include these, not sure why they should be included by the above
#include <libical/icaltime.h>
#include <cairo/cairo.h>


#include <oobs/oobs-timeconfig.h>

#include "datetime-interface.h"
#include "dbus-shared.h"

static void geo_create_client (GeoclueMaster * master, GeoclueMasterClient * client, gchar * path, GError * error, gpointer user_data);
static gboolean update_appointment_menu_items (gpointer user_data);
static void setup_timer (void);
static void geo_client_invalid (GeoclueMasterClient * client, gpointer user_data);
static void geo_address_change (GeoclueMasterClient * client, gchar * a, gchar * b, gchar * c, gchar * d, gpointer user_data);

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
static DbusmenuMenuitem * add_appointment = NULL;
// static DbusmenuMenuitem * add_location = NULL;
static GList			* appointments = NULL;
static ECal				* ecal = NULL;
static const gchar		* ecal_timezone = NULL;
static icaltimezone     *tzone;

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
		dbusmenu_menuitem_property_set_bool(add_appointment, DBUSMENU_MENUITEM_PROP_ENABLED, TRUE);
		dbusmenu_menuitem_property_set_bool(add_appointment, DBUSMENU_MENUITEM_PROP_VISIBLE, TRUE);

		GError *gerror = NULL;
		// TODO: In reality we should iterate sources of calendar, but getting the local one doens't lag for > a minute
		g_debug("Setting up ecal.");
		if (!ecal)
			ecal = e_cal_new_system_calendar();
	
		if (!ecal) {
			g_debug("e_cal_new_system_calendar failed");
			ecal = NULL;
		}
		g_debug("Open calendar.");
		if (!e_cal_open(ecal, FALSE, &gerror) ) {
			g_debug("e_cal_open: %s\n", gerror->message);
			g_free(ecal);
			ecal = NULL;
		}
		g_debug("Get calendar timezone.");
		if (!e_cal_get_timezone(ecal, "UTC", &tzone, &gerror)) {
			g_debug("failed to get time zone\n");
			g_free(ecal);
			ecal = NULL;
		}
	
		/* This timezone represents the timezone of the calendar, this might be different to the current UTC offset.
		 * this means we'll have some geoclue interaction going on, and possibly the user will be involved in setting
		 * their location manually, case in point: trains have satellite links which often geoclue to sweden,
		 * this shouldn't automatically set the location and mess up all the appointments for the user.
		 */
		if (ecal) ecal_timezone = icaltimezone_get_tzid(tzone);
	
		update_appointment_menu_items(NULL);
		g_signal_connect(root, DBUSMENU_MENUITEM_SIGNAL_ABOUT_TO_SHOW, G_CALLBACK(update_appointment_menu_items), NULL);		

		g_free(evo);
	} else {
		g_debug("Unable to find calendar app.");
		dbusmenu_menuitem_property_set_bool(calendar, DBUSMENU_MENUITEM_PROP_VISIBLE, FALSE);
		dbusmenu_menuitem_property_set_bool(add_appointment, DBUSMENU_MENUITEM_PROP_VISIBLE, FALSE);
	}

	return FALSE;
}

/*
static gboolean
update_timezone_menu_items(gpointer user_data) {
	// Get the current location as specified by the user as a place name and time and add it,
	// Get the location from geoclue as a place and time and add it,
	// Get the evolution calendar timezone as a place and time and add it,
	// Get the current timezone that the clock uses and select that
	// Iterate over configured places and add any which aren't already listed
	// Hook up each of these to setting the time/current timezone
	return FALSE;
}
*/

// Compare function for g_list_sort of ECalComponent objects
static gint 
compare_appointment_items (ECalComponent *a,
                           ECalComponent *b) {
                                       
	ECalComponentDateTime datetime_a, datetime_b;
	struct tm tm_a, tm_b;
	time_t t_a, t_b;
	gint retval = 0;
	
	ECalComponentVType vtype = e_cal_component_get_vtype (a);
	if (vtype == E_CAL_COMPONENT_EVENT)
		e_cal_component_get_dtstart (a, &datetime_a);
	else
	    e_cal_component_get_due (a, &datetime_a);
	tm_a = icaltimetype_to_tm(datetime_a.value);
	t_a = mktime(&tm_a);
	
	vtype = e_cal_component_get_vtype (b);
	if (vtype == E_CAL_COMPONENT_EVENT)
		e_cal_component_get_dtstart (b, &datetime_b);
	else
	    e_cal_component_get_due (b, &datetime_b);
	tm_b = icaltimetype_to_tm(datetime_b.value);
	t_b = mktime(&tm_b);

	// Compare datetime_a and datetime_b, newest first in this sort.
	if (t_a > t_b) retval = 1;
	else if (t_a < t_b) retval = -1;
	
	e_cal_component_free_datetime (&datetime_a);
	e_cal_component_free_datetime (&datetime_b);
	return retval;
}

/* Populate the menu with todays, next 5 appointments. 
 * we should hook into the ABOUT TO SHOW signal and use that to update the appointments.
 * Experience has shown that caldav's and webcals can be slow to load from eds
 * this is a problem mainly on the EDS side of things, not ours. 
 */
static gboolean
update_appointment_menu_items (gpointer user_data) {
	if (!ecal) return FALSE;
	// FFR: we should take into account short term timers, for instance
	// tea timers, pomodoro timers etc... that people may add, this is hinted to in the spec.
	time_t t1, t2;
	gchar *query, *is, *ie, *ad;
	GList *objects = NULL, *l;
	GError *gerror = NULL;
	gint i;
	gint width, height;
	
	time(&t1);
	time(&t2);
	t2 += (time_t) (7 * 24 * 60 * 60); /* 7 days ahead of now */

	is = isodate_from_time_t(t1);
	ie = isodate_from_time_t(t2);
	
	// FIXME can we put a limit on the number of results? Or if not complete, or is event/todo? Or sort it by date?
	query = g_strdup_printf("(occur-in-time-range? (make-time\"%s\") (make-time\"%s\"))", is, ie);
	g_debug("Getting objects with query: %s", query);
	if (!e_cal_get_object_list_as_comp(ecal, query, &objects, &gerror)) {
		g_debug("Failed to get objects\n");
		g_free(ecal);
		ecal = NULL;
		return FALSE;
	}
	g_debug("Number of objects returned: %d", g_list_length(objects));
	gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, &width, &height);

	/* Remove all of the previous appointments */
	if (appointments != NULL) {
		g_debug("Freeing old appointments");
		while (appointments != NULL) {
			DbusmenuMenuitem * litem =  DBUSMENU_MENUITEM(appointments->data);
			g_debug("Freeing old appointment: %p", litem);
			// Remove all the existing menu items which are in appointments.
			appointments = g_list_remove(appointments, litem);
			dbusmenu_menuitem_child_delete(root, DBUSMENU_MENUITEM(litem));
			g_object_unref(G_OBJECT(litem));
		}
	}
	
	// Sort the list see above FIXME regarding queries
	objects = g_list_sort(objects, (GCompareFunc) compare_appointment_items);
	i = 0;
	for (l = objects; l; l = l->next) {
		ECalComponent *ecalcomp = l->data;
		ECalComponentText valuetext;
		ECalComponentDateTime datetime;
		icaltimezone *appointment_zone = NULL;
		icalproperty_status status;
		gchar *summary, *cmd;
		char right[20];
		//const gchar *uri;
		struct tm tmp_tm;
		DbusmenuMenuitem * item;

		ECalComponentVType vtype = e_cal_component_get_vtype (ecalcomp);

		// See above FIXME regarding query result
		// If it's not an event or todo, continue no-increment
        if (vtype != E_CAL_COMPONENT_EVENT && vtype != E_CAL_COMPONENT_TODO)
			continue;

		// See above FIXME regarding query result
		// if the status is completed, continue no-increment
		e_cal_component_get_status (ecalcomp, &status);
		if (status == ICAL_STATUS_COMPLETED || status == ICAL_STATUS_CANCELLED)
			continue;

		item = dbusmenu_menuitem_new();
		dbusmenu_menuitem_property_set       (item, DBUSMENU_MENUITEM_PROP_TYPE, APPOINTMENT_MENUITEM_TYPE);
		dbusmenu_menuitem_property_set_bool  (item, DBUSMENU_MENUITEM_PROP_ENABLED, TRUE);
		dbusmenu_menuitem_property_set_bool  (item, DBUSMENU_MENUITEM_PROP_VISIBLE, TRUE);
	
		g_debug("Start Object");
        // Label text        
		e_cal_component_get_summary (ecalcomp, &valuetext);
		summary = g_strdup (valuetext.value);

		dbusmenu_menuitem_property_set (item, APPOINTMENT_MENUITEM_PROP_LABEL, summary);
		g_debug("Summary: %s", summary);
		g_free (summary);
		
		// Due text
		if (vtype == E_CAL_COMPONENT_EVENT)
			e_cal_component_get_dtstart (ecalcomp, &datetime);
		else
		    e_cal_component_get_due (ecalcomp, &datetime);
		
		// FIXME need to get the timezone of the above datetime, 
		// and get the icaltimezone of the geoclue timezone/selected timezone (whichever is preferred)
		if (!datetime.value) {
			g_free(item);
			continue;
		}
		
		if (!appointment_zone || datetime.value->is_date) { // If it's today put in the current timezone?
			appointment_zone = tzone;
		}
		tmp_tm = icaltimetype_to_tm_with_zone (datetime.value, appointment_zone, tzone);
		
		g_debug("Generate time string");
		// Get today
		time_t curtime = time(NULL);
  		struct tm* today = localtime(&curtime);
		if (today->tm_mday == tmp_tm.tm_mday && 
		    today->tm_mon == tmp_tm.tm_mon && 
		    today->tm_year == tmp_tm.tm_year)
			strftime(right, 20, "%l:%M %P", &tmp_tm);
		else
			strftime(right, 20, "%a %l:%M %P", &tmp_tm);
			
		g_debug("Appointment time: %s", right);
		dbusmenu_menuitem_property_set (item, APPOINTMENT_MENUITEM_PROP_RIGHT, right);
		
		e_cal_component_free_datetime (&datetime);
		
		ad = is = isodate_from_time_t(mktime(&tmp_tm));
		
		// Now we pull out the URI for the calendar event and try to create a URI that'll work when we execute evolution
		// FIXME Because the URI stuff is really broken, we're going to open the calendar at todays date instead
		
		//e_cal_component_get_uid(ecalcomp, &uri);
		cmd = g_strconcat("evolution calendar:///?startdate=", ad, NULL);
		g_signal_connect (G_OBJECT(item), DBUSMENU_MENUITEM_SIGNAL_ITEM_ACTIVATED,
						  G_CALLBACK (activate_cb), cmd);
		
		g_debug("Command to Execute: %s", cmd);
		
		ESource *source = e_cal_get_source (ecal);
        //e_source_get_color (source, &source_color); api has been changed
        const gchar *color_spec = e_source_peek_color_spec(source);
        GdkColor color;
        g_debug("Colour to use: %s", color_spec);
			
		// Draw the correct icon for the appointment type and then tint it using mask fill.
		// For now we'll create a circle
        if (color_spec != NULL) {
        	gdk_color_parse (color_spec, &color);
        	
			cairo_surface_t *cs = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
			cairo_t *cr = cairo_create(cs);
			cairo_arc (cr, width/2, height/2, width/2, 0, 2 * M_PI);
			gdk_cairo_set_source_color(cr, &color);
			cairo_fill(cr);
		
			GdkPixbuf * pixbuf = gdk_pixbuf_get_from_drawable(NULL, (GdkDrawable*)cs, 
				gdk_colormap_new(gdk_drawable_get_visual((GdkDrawable*)cs), TRUE), 0,0,0,0, width, height);
			cairo_destroy(cr);
			
			dbusmenu_menuitem_property_set_image (item, APPOINTMENT_MENUITEM_PROP_ICON, pixbuf);
		}
		dbusmenu_menuitem_child_add_position (root, item, 4+i);
		appointments = g_list_append         (appointments, item); // Keep track of the items here to make them east to remove
		g_debug("Adding appointment: %p", item);
		
		if (i == 4) break; // See above FIXME regarding query result limit
		i++;
	}
	g_list_free(objects);
	g_debug("End of objects");
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

	add_appointment = dbusmenu_menuitem_new();
	dbusmenu_menuitem_property_set     (add_appointment, DBUSMENU_MENUITEM_PROP_LABEL, _("Add Appointment"));
	dbusmenu_menuitem_property_set_bool(add_appointment, DBUSMENU_MENUITEM_PROP_ENABLED, FALSE);
	dbusmenu_menuitem_property_set_bool(add_appointment, DBUSMENU_MENUITEM_PROP_VISIBLE, TRUE);
	g_signal_connect(G_OBJECT(add_appointment), DBUSMENU_MENUITEM_SIGNAL_ITEM_ACTIVATED, G_CALLBACK(activate_cb), "evolution -c calendar");
	dbusmenu_menuitem_child_add_position (root, add_appointment, 4);
	
	separator = dbusmenu_menuitem_new();
	dbusmenu_menuitem_property_set(separator, DBUSMENU_MENUITEM_PROP_TYPE, DBUSMENU_CLIENT_TYPES_SEPARATOR);
	dbusmenu_menuitem_child_append(root, separator);

	tzchange = dbusmenu_menuitem_new();
	dbusmenu_menuitem_property_set(tzchange, DBUSMENU_MENUITEM_PROP_LABEL, "Set specific timezone");
	dbusmenu_menuitem_property_set_bool(tzchange, DBUSMENU_MENUITEM_PROP_VISIBLE, FALSE);
	g_signal_connect(G_OBJECT(tzchange), DBUSMENU_MENUITEM_SIGNAL_ITEM_ACTIVATED, G_CALLBACK(quick_set_tz), NULL);
	dbusmenu_menuitem_child_append(root, tzchange);
	check_timezone_sync();

	separator = dbusmenu_menuitem_new();
	dbusmenu_menuitem_property_set(separator, DBUSMENU_MENUITEM_PROP_TYPE, DBUSMENU_CLIENT_TYPES_SEPARATOR);
	dbusmenu_menuitem_child_append(root, separator);

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

	check_timezone_sync();

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

	check_timezone_sync();

	return;
}

/* Callback from creating the client */
static void
geo_create_client (GeoclueMaster * master, GeoclueMasterClient * client, gchar * path, GError * error, gpointer user_data)
{
	g_debug("Created Geoclue client at: %s", path);

	geo_master = client;

	if (geo_master != NULL) {
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

	geo_address_clean();
	geo_client_clean();

	g_object_unref(G_OBJECT(master));
	g_object_unref(G_OBJECT(dbus));
	g_object_unref(G_OBJECT(service));
	g_object_unref(G_OBJECT(server));
	g_object_unref(G_OBJECT(root));

	return 0;
}
