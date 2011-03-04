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
#include <libedataserverui/e-passwords.h>
// Other users of ecal seem to also include these, not sure why they should be included by the above
#include <libical/icaltime.h>
#include <cairo/cairo.h>

#include <oobs/oobs-timeconfig.h>

#include "datetime-interface.h"
#include "dbus-shared.h"
#include "settings-shared.h"
#include "utils.h"


static void geo_create_client (GeoclueMaster * master, GeoclueMasterClient * client, gchar * path, GError * error, gpointer user_data);
static gboolean update_appointment_menu_items (gpointer user_data);
static gboolean update_timezone_menu_items(gpointer user_data);
static void setup_timer (void);
static void geo_client_invalid (GeoclueMasterClient * client, gpointer user_data);
static void geo_address_change (GeoclueMasterClient * client, gchar * a, gchar * b, gchar * c, gchar * d, gpointer user_data);

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
static DbusmenuMenuitem * geo_location = NULL;
static DbusmenuMenuitem * current_location = NULL;
//static DbusmenuMenuitem * ecal_location = NULL;
static DbusmenuMenuitem * add_appointment = NULL;
static GList			* appointments = NULL;
static GList			* dconflocations = NULL;
static GList			* comp_instances = NULL;
static gboolean           updating_appointments = FALSE;
GSettings *conf;


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

static void
set_timezone_label (DbusmenuMenuitem * mi, const gchar * location)
{
	gchar * zone, * name;
	split_settings_location (location, &zone, &name);

	dbusmenu_menuitem_property_set (mi, TIMEZONE_MENUITEM_PROP_NAME, name);
	dbusmenu_menuitem_property_set (mi, TIMEZONE_MENUITEM_PROP_ZONE, zone);

	g_free (zone);
	g_free (name);
}

/* Check to see if our timezones are the same */
static void
check_timezone_sync (void) {
	gchar * label;
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

	gboolean show = g_settings_get_boolean (conf, SETTINGS_SHOW_LOCATIONS_S);

	if (geo_location != NULL && current_location != NULL) {
		g_debug("Got timezone %s", current_timezone);
		g_debug("Got timezone %s", geo_timezone);
		// Show neither current location nor geo location if both are the same
		// however, we want to set their time and label accordingly
		if (in_sync) {
			if (current_timezone == NULL && geo_timezone == NULL) {
				dbusmenu_menuitem_property_set_bool(locations_separator, DBUSMENU_MENUITEM_PROP_VISIBLE, FALSE);
				dbusmenu_menuitem_property_set_bool (current_location, DBUSMENU_MENUITEM_PROP_VISIBLE, FALSE);
				dbusmenu_menuitem_property_set_bool (geo_location, DBUSMENU_MENUITEM_PROP_VISIBLE, FALSE);
				update_timezone_menu_items(NULL); // Update the timezone menu items 
				return;
			}
			
			dbusmenu_menuitem_property_set_bool (locations_separator, DBUSMENU_MENUITEM_PROP_VISIBLE, FALSE);
			dbusmenu_menuitem_property_set_bool (current_location, DBUSMENU_MENUITEM_PROP_VISIBLE, FALSE);
			dbusmenu_menuitem_property_set_bool (current_location, DBUSMENU_MENUITEM_PROP_ENABLED, TRUE);
			dbusmenu_menuitem_property_set_bool (geo_location, DBUSMENU_MENUITEM_PROP_VISIBLE, FALSE);
			dbusmenu_menuitem_property_set_bool (geo_location, DBUSMENU_MENUITEM_PROP_ENABLED, TRUE);
			
			if (current_timezone != NULL) {
				label = current_timezone;
			} else {
				label = geo_timezone;
			}
			
			if (label != NULL) {
				// TODO work out the current location name in a nice way
				set_timezone_label (current_location, label);
				// TODO work out the current time at that location 
				dbusmenu_menuitem_property_set_bool (current_location, DBUSMENU_MENUITEM_PROP_VISIBLE, show);
				dbusmenu_menuitem_property_set_bool(current_location, TIMEZONE_MENUITEM_PROP_RADIO, TRUE);
			} else {
				g_debug("Label for current location is null, this shouldn't happen");
			}
			if (geo_timezone != NULL) {	
				// TODO work out the geo location name in a nice way
				set_timezone_label (geo_location, geo_timezone);
				// TODO work out the current time at that location 
				dbusmenu_menuitem_property_set_bool (geo_location, DBUSMENU_MENUITEM_PROP_VISIBLE, show);
			}
		} else {
			// TODO work out the geo location name in a nice way
			set_timezone_label (geo_location, geo_timezone);
			// TODO work out the current time at that location 
			dbusmenu_menuitem_property_set_bool(geo_location, DBUSMENU_MENUITEM_PROP_VISIBLE, show);
			
			// TODO work out the current location name in a nice way
			set_timezone_label (current_location, current_timezone);
			// TODO work out the current time at that location 
			dbusmenu_menuitem_property_set_bool(current_location, TIMEZONE_MENUITEM_PROP_RADIO, TRUE);
			dbusmenu_menuitem_property_set_bool(current_location, DBUSMENU_MENUITEM_PROP_VISIBLE, show);
			dbusmenu_menuitem_property_set_bool(locations_separator, DBUSMENU_MENUITEM_PROP_VISIBLE, show);
		}
	}
	g_debug("Finished checking timezone sync");
	update_timezone_menu_items(NULL); // Update the timezone menu items 

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

    if (error != NULL) g_error_free(error);
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

static gboolean
month_changed_cb (DbusmenuMenuitem * menuitem, GVariant *variant, guint timestamp)
{
	// BLOCKED: We're not getting the signal from calendar the ido calendar menuitem
	// TODO: * Decode the month/year from the string we received
	//       * Check what our current month/year are
	//		 * Set some globals so when we-re-run update appointment menu items it gets the right start date
	//		 * update appointment menu items
	g_debug("Received month changed : %s", g_variant_get_string(variant, NULL));
	return TRUE;
}

static guint ecaltimer = 0;

static void
start_ecal_timer(void)
{
	if (ecaltimer != 0) g_source_remove(ecaltimer);
	if (update_appointment_menu_items(NULL))
		ecaltimer = g_timeout_add_seconds(60*5, update_appointment_menu_items, NULL); 	
}

static void
stop_ecal_timer(void)
{
	if (ecaltimer != 0) g_source_remove(ecaltimer);
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
		stop_ecal_timer();
	}
}

/* Looks for the calendar application and enables the item if
   we have one, starts ecal timer if events are turned on */
static gboolean
check_for_calendar (gpointer user_data)
{
	g_return_val_if_fail (calendar != NULL, FALSE);
	
	if (!g_settings_get_boolean(conf, SETTINGS_SHOW_CALENDAR_S)) return FALSE;

	gchar *evo = g_find_program_in_path("evolution");
	if (evo != NULL) {
		g_debug("Found the calendar application: %s", evo);
		dbusmenu_menuitem_property_set_bool(calendar, DBUSMENU_MENUITEM_PROP_ENABLED, TRUE);
		dbusmenu_menuitem_property_set_bool(calendar, DBUSMENU_MENUITEM_PROP_VISIBLE, TRUE);

		events_separator = dbusmenu_menuitem_new();
		dbusmenu_menuitem_property_set(events_separator, DBUSMENU_MENUITEM_PROP_TYPE, DBUSMENU_CLIENT_TYPES_SEPARATOR);
		dbusmenu_menuitem_child_add_position(root, events_separator, 2);
		add_appointment = dbusmenu_menuitem_new();
		dbusmenu_menuitem_property_set (add_appointment, DBUSMENU_MENUITEM_PROP_LABEL, _("Add Appointment"));
		dbusmenu_menuitem_property_set_bool(add_appointment, DBUSMENU_MENUITEM_PROP_ENABLED, TRUE);
		g_signal_connect(G_OBJECT(add_appointment), DBUSMENU_MENUITEM_SIGNAL_ITEM_ACTIVATED, G_CALLBACK(activate_cb), "evolution -c calendar");
		dbusmenu_menuitem_child_add_position (root, add_appointment, 3);


		if (g_settings_get_boolean(conf, SETTINGS_SHOW_EVENTS_S)) {
			dbusmenu_menuitem_property_set_bool(add_appointment, DBUSMENU_MENUITEM_PROP_VISIBLE, TRUE);
			dbusmenu_menuitem_property_set_bool(events_separator, DBUSMENU_MENUITEM_PROP_VISIBLE, TRUE);
			start_ecal_timer();
		} else {
			dbusmenu_menuitem_property_set_bool(add_appointment, DBUSMENU_MENUITEM_PROP_VISIBLE, FALSE);
			dbusmenu_menuitem_property_set_bool(events_separator, DBUSMENU_MENUITEM_PROP_VISIBLE, FALSE);
			stop_ecal_timer();
		}
		
		// Connect to event::month-changed 
		g_signal_connect(calendar, "event::month-changed", G_CALLBACK(month_changed_cb), NULL);
		g_free(evo);
	} else {
		g_debug("Unable to find calendar app.");
		dbusmenu_menuitem_property_set_bool(calendar, DBUSMENU_MENUITEM_PROP_VISIBLE, FALSE);
		dbusmenu_menuitem_property_set_bool(add_appointment, DBUSMENU_MENUITEM_PROP_VISIBLE, FALSE);
	}

	return FALSE;
}


static gboolean
update_timezone_menu_items(gpointer user_data) {
	g_debug("Updating timezone menu items");

	gchar ** locations = g_settings_get_strv(conf, SETTINGS_LOCATIONS_S);

	if (locations == NULL) { 
		g_debug("No locations configured (NULL)");
		return FALSE;
	} 
	guint len = g_strv_length(locations);
	DbusmenuMenuitem *item;
	gint i, offset;
	
	/* Remove all of the previous locations */
	if (dconflocations != NULL) {
		while (dconflocations != NULL) {
			DbusmenuMenuitem * litem =  DBUSMENU_MENUITEM(dconflocations->data);
			// Remove all the existing menu items which are in dconflocations.
			dconflocations = g_list_remove(dconflocations, litem);
			dbusmenu_menuitem_child_delete(root, DBUSMENU_MENUITEM(litem));
			g_object_unref(G_OBJECT(litem));
		}
	}
	
	gboolean show = g_settings_get_boolean (conf, SETTINGS_SHOW_LOCATIONS_S);

	if (len > 0) {
		dbusmenu_menuitem_property_set_bool (locations_separator, DBUSMENU_MENUITEM_PROP_VISIBLE, show);
		dbusmenu_menuitem_property_set_bool (current_location, DBUSMENU_MENUITEM_PROP_VISIBLE, show);
		dbusmenu_menuitem_property_set_bool (current_location, DBUSMENU_MENUITEM_PROP_ENABLED, TRUE);
	} else {
		g_debug("No locations configured (Empty List)");
		return FALSE;
	}
	
	offset = dbusmenu_menuitem_get_position (current_location, root)+1;
	for (i = 0; i < len; i++) {
		// Iterate over configured places and add any which aren't already listed
		if (g_strcmp0(locations[i], current_timezone) != 0 &&
		    g_strcmp0(locations[i], geo_timezone) != 0) {
			g_debug("Adding timezone in update_timezones %s", locations[i]);
			item = dbusmenu_menuitem_new();
			dbusmenu_menuitem_property_set      (item, DBUSMENU_MENUITEM_PROP_TYPE, TIMEZONE_MENUITEM_TYPE);
			set_timezone_label (item, locations[i]);
			dbusmenu_menuitem_property_set_bool (item, TIMEZONE_MENUITEM_PROP_RADIO, FALSE);
			dbusmenu_menuitem_property_set_bool (item, DBUSMENU_MENUITEM_PROP_ENABLED, TRUE);
			dbusmenu_menuitem_property_set_bool (item, DBUSMENU_MENUITEM_PROP_VISIBLE, show);
			dbusmenu_menuitem_child_add_position (root, item, offset++);
			dconflocations = g_list_append(dconflocations, item);
		}
	}
	g_strfreev (locations);
	return FALSE;
}

// Authentication function
static gchar *
auth_func (ECal *ecal, 
           const gchar *prompt, 
           const gchar *key, 
           gpointer user_data)
{
	gboolean remember; // TODO: Is this useful?  Should we be storing it somewhere?
	ESource *source = e_cal_get_source (ecal);
	gchar *auth_domain = e_source_get_duped_property (source, "auth-domain");

	const gchar *component_name;
	if (auth_domain) component_name = auth_domain;
	else component_name = "Calendar";
	
	gchar *password = e_passwords_get_password (component_name, key);
	
	if (password == NULL) {
		password = e_passwords_ask_password (
			_("Enter password"),
			component_name, key, prompt,
			E_PASSWORDS_REMEMBER_FOREVER |
			E_PASSWORDS_SECRET |
			E_PASSWORDS_ONLINE,
			&remember, NULL);
	}
	
	g_free (auth_domain);

	return password;
}

static gint
compare_comp_instances (gconstpointer a, 
                        gconstpointer b)
{
        const struct comp_instance *ci_a = a;
        const struct comp_instance *ci_b = b;
        time_t d = ci_a->start - ci_b->start;
		if (d < 0) return -1;
		else if (d > 0) return 1; 
		return 0;
}

static gboolean
populate_appointment_instances (ECalComponent *comp,
                                time_t instance_start,
                                time_t instance_end,
                                gpointer data)
{
	g_debug("Appending item %p", comp);
	
	ECalComponentVType vtype = e_cal_component_get_vtype (comp);
	if (vtype != E_CAL_COMPONENT_EVENT && vtype != E_CAL_COMPONENT_TODO) return FALSE;
	
	icalproperty_status status;
	e_cal_component_get_status (comp, &status);
	if (status == ICAL_STATUS_COMPLETED || status == ICAL_STATUS_CANCELLED) return FALSE;
	
	g_object_ref(comp);
	
	ECalComponentDateTime datetime;
	icaltimezone *appointment_zone = NULL;
	icaltimezone *current_zone = NULL;
	
	if (vtype == E_CAL_COMPONENT_EVENT)
		e_cal_component_get_dtstart (comp, &datetime);
	else
	    e_cal_component_get_due (comp, &datetime);

	appointment_zone = icaltimezone_get_builtin_timezone_from_tzid(datetime.tzid);
	current_zone = icaltimezone_get_builtin_timezone_from_tzid(current_timezone);
	if (!appointment_zone || datetime.value->is_date) { // If it's today put in the current timezone?
		appointment_zone = current_zone;
	}
	
	// TODO: Convert the timezone into a 3 letter abbreviation if it's different to current_timezone
	// TODO: Add the appointment timezone to the list if it's not already there. 
	
	struct comp_instance *ci;
	ci = g_new (struct comp_instance, 1);
	
	g_debug("Using times start %s, end %s", ctime(&instance_start), ctime(&instance_end));
	
	ci->comp = comp;
	ci->source = E_SOURCE(data);
	ci->start = instance_start;
	ci->end = instance_end;
	
	comp_instances = g_list_append(comp_instances, ci);
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
	if (calendar == NULL) return FALSE;
	if (!g_settings_get_boolean(conf, SETTINGS_SHOW_EVENTS_S)) return FALSE;
	if (updating_appointments) return TRUE;
	updating_appointments = TRUE;
	
	time_t t1, t2;
	gchar *ad;
	GList *l;
	//GList *allobjects = NULL;
	GSList *g;
	GError *gerror = NULL;
	gint i;
	gint width, height;
	ESourceList * sources = NULL;
	
	time(&t1);
	time(&t2);
	t2 += (time_t) (7 * 24 * 60 * 60); /* 7 days ahead of now, we actually need number_of_days_in_this_month */

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
	
	// TODO Remove all highlights from the calendar widget
	
	if (!e_cal_get_sources(&sources, E_CAL_SOURCE_TYPE_EVENT, &gerror)) {
		g_debug("Failed to get ecal sources\n");
		return FALSE;
	}
	
	// Free comp_instances if not NULL
	if (comp_instances != NULL) {
		g_debug("Freeing comp_instances: may be an overlap\n");
		for (l = comp_instances; l; l = l->next) {
			const struct comp_instance *ci = l->data;
			g_object_unref(ci->comp);
			g_list_free(comp_instances);
			comp_instances = NULL;
		}
	}
	
	// Generate instances for all sources
	for (g = e_source_list_peek_groups (sources); g; g = g->next) {
		ESourceGroup *group = E_SOURCE_GROUP (g->data);
		GSList *s;
		
		for (s = e_source_group_peek_sources (group); s; s = s->next) {
			ESource *source = E_SOURCE (s->data);
			//g_signal_connect (G_OBJECT(source), "changed", G_CALLBACK (update_appointment_menu_items), NULL);
			ECal *ecal = e_cal_new(source, E_CAL_SOURCE_TYPE_EVENT);
			e_cal_set_auth_func (ecal, (ECalAuthFunc) auth_func, NULL);
			
			if (!e_cal_open(ecal, FALSE, &gerror)) {
				g_debug("Failed to get ecal sources %s", gerror->message);
				g_error_free(gerror);
				gerror = NULL;
				continue;
        	}
			
			e_cal_generate_instances (ecal, t1, t2, (ECalRecurInstanceFn) populate_appointment_instances, (gpointer) source);
		}
	}
	g_debug("Number of ECalComponents returned: %d", g_list_length(comp_instances));
	GList *sorted_comp_instances = g_list_sort(comp_instances, compare_comp_instances);
	comp_instances = NULL;
	
	gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, &width, &height);
	if (width == 0) width = 12;
	if (height == 0) height = 12;
	
	i = 0;
	for (l = sorted_comp_instances; l; l = l->next) {
		struct comp_instance *ci = l->data;
		ECalComponent *ecalcomp = ci->comp;
		ECalComponentText valuetext;
		gchar *summary, *cmd;
		char right[20];
		//const gchar *uri;
		DbusmenuMenuitem * item;

		g_debug("Start Object %p", ecalcomp);
		
		// TODO Mark days
		
		if (i >= 5) continue;
		i++;
		
		item = dbusmenu_menuitem_new();
		dbusmenu_menuitem_property_set       (item, DBUSMENU_MENUITEM_PROP_TYPE, APPOINTMENT_MENUITEM_TYPE);
		dbusmenu_menuitem_property_set_bool  (item, DBUSMENU_MENUITEM_PROP_ENABLED, TRUE);
		dbusmenu_menuitem_property_set_bool  (item, DBUSMENU_MENUITEM_PROP_VISIBLE, TRUE);
	
        // Label text        
		e_cal_component_get_summary (ecalcomp, &valuetext);
		summary = g_strdup (valuetext.value);

		dbusmenu_menuitem_property_set (item, APPOINTMENT_MENUITEM_PROP_LABEL, summary);
		g_debug("Summary: %s", summary);
		g_free (summary);

		// Due text
		ECalComponentVType vtype = e_cal_component_get_vtype (ecalcomp);
		
		// Get today
		time_t curtime = time(NULL);
  		struct tm *today = localtime(&curtime);
		
		int mday = today->tm_mday;
		int mon = today->tm_mon;
		int year = today->tm_year;
		
		struct tm *due;
		if (vtype == E_CAL_COMPONENT_EVENT) due = localtime(&ci->start);
		else if (vtype == E_CAL_COMPONENT_TODO) due = localtime(&ci->end);
		else continue;

		int dmday = due->tm_mday;
		int dmon = due->tm_mon;
		int dyear = due->tm_year;
		
		if ((mday == dmday) && (mon == dmon) && (year == dyear))
			strftime(right, 20, "%l:%M %p", due);
		else
			strftime(right, 20, "%a %l:%M %p", due);
			
		g_debug("Appointment time: %s", right);
		dbusmenu_menuitem_property_set (item, APPOINTMENT_MENUITEM_PROP_RIGHT, right);
		
		// Now we pull out the URI for the calendar event and try to create a URI that'll work when we execute evolution
		// FIXME Because the URI stuff is really broken, we're going to open the calendar at todays date instead
		//e_cal_component_get_uid(ecalcomp, &uri);
		ad = isodate_from_time_t(mktime(due));
		cmd = g_strconcat("evolution calendar:///?startdate=", ad, NULL);
		g_signal_connect (G_OBJECT(item), DBUSMENU_MENUITEM_SIGNAL_ITEM_ACTIVATED,
						  G_CALLBACK (activate_cb), cmd);
		
		g_debug("Command to Execute: %s", cmd);

        const gchar *color_spec = e_source_peek_color_spec(ci->source);
        g_debug("Colour to use: %s", color_spec);
			
		// Draw the correct icon for the appointment type and then tint it using mask fill.
		// For now we'll create a circle
        if (color_spec != NULL) {
        	// Fixme causes segfault, but we have colours now yay!
        	GdkColor color;
        	gdk_color_parse (color_spec, &color);	
        	g_debug("Creating a cairo surface\n    size, %d by %d", width, height);         
        	cairo_surface_t *surface = cairo_image_surface_create( CAIRO_FORMAT_ARGB32, width, height ); 

    		cairo_t *cr = cairo_create(surface);
			gdk_cairo_set_source_color(cr, &color);
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
			} else {
				g_debug("Creating pixbuf from surface failed\n    Couldn't create new pixbuf for size, %d by %d", width, height);
			}
			cairo_surface_destroy (surface);
			cairo_destroy(cr);
		}
		dbusmenu_menuitem_child_add_position (root, item, 2+i);
		appointments = g_list_append         (appointments, item); // Keep track of the items here to make them east to remove
		g_debug("Adding appointment: %p", item);
	}
	
    if (gerror != NULL) g_error_free(gerror);
	for (l = sorted_comp_instances; l; l = l->next) { 
		const struct comp_instance *ci = l->data;
		g_object_unref(ci->comp);
		g_list_free(sorted_comp_instances);
	}
	
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

	gchar * timeadmin = g_find_program_in_path("indicator-datetime-preferences");
	if (timeadmin != NULL) {
		g_debug("Found the indicator-datetime-preferences application: %s", timeadmin);
		dbusmenu_menuitem_property_set_bool(settings, DBUSMENU_MENUITEM_PROP_ENABLED, TRUE);
		g_free(timeadmin);
	} else {
		g_debug("Unable to find indicator-datetime-preferences app.");
		dbusmenu_menuitem_property_set_bool(settings, DBUSMENU_MENUITEM_PROP_ENABLED, FALSE);
	}

	return FALSE;
}

static void
show_locations_changed (void)
{
	/* Re-calculate */
	check_timezone_sync();
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
	
	locations_separator = dbusmenu_menuitem_new();
	dbusmenu_menuitem_property_set(locations_separator, DBUSMENU_MENUITEM_PROP_TYPE, DBUSMENU_CLIENT_TYPES_SEPARATOR);
	dbusmenu_menuitem_property_set_bool (locations_separator, DBUSMENU_MENUITEM_PROP_VISIBLE, FALSE);
	dbusmenu_menuitem_child_append(root, locations_separator);

	geo_location = dbusmenu_menuitem_new();
	dbusmenu_menuitem_property_set      (geo_location, DBUSMENU_MENUITEM_PROP_TYPE, TIMEZONE_MENUITEM_TYPE);
	set_timezone_label (geo_location, "");
	dbusmenu_menuitem_property_set_bool (geo_location, DBUSMENU_MENUITEM_PROP_ENABLED, FALSE);
	dbusmenu_menuitem_property_set_bool (geo_location, DBUSMENU_MENUITEM_PROP_VISIBLE, TRUE);
	dbusmenu_menuitem_child_append(root, geo_location);

	current_location = dbusmenu_menuitem_new();
	dbusmenu_menuitem_property_set      (current_location, DBUSMENU_MENUITEM_PROP_TYPE, TIMEZONE_MENUITEM_TYPE);
	set_timezone_label (current_location, "");
	dbusmenu_menuitem_property_set_bool (current_location, DBUSMENU_MENUITEM_PROP_ENABLED, FALSE);
	dbusmenu_menuitem_property_set_bool (current_location, DBUSMENU_MENUITEM_PROP_VISIBLE, FALSE);
	dbusmenu_menuitem_child_append(root, current_location);
	
	check_timezone_sync();
	
	g_signal_connect (conf, "changed::" SETTINGS_SHOW_LOCATIONS_S, G_CALLBACK (show_locations_changed), NULL);
	g_signal_connect (conf, "changed::" SETTINGS_LOCATIONS_S, G_CALLBACK (show_locations_changed), NULL);
	g_signal_connect (conf, "changed::" SETTINGS_SHOW_EVENTS_S, G_CALLBACK (show_events_changed), NULL);

	DbusmenuMenuitem * separator = dbusmenu_menuitem_new();
	dbusmenu_menuitem_property_set(separator, DBUSMENU_MENUITEM_PROP_TYPE, DBUSMENU_CLIENT_TYPES_SEPARATOR);
	dbusmenu_menuitem_child_append(root, separator);

	settings = dbusmenu_menuitem_new();
	dbusmenu_menuitem_property_set     (settings, DBUSMENU_MENUITEM_PROP_LABEL, _("Time & Date Settings..."));
	/* insensitive until we check for available apps */
	dbusmenu_menuitem_property_set_bool(settings, DBUSMENU_MENUITEM_PROP_ENABLED, FALSE);
	g_signal_connect(G_OBJECT(settings), DBUSMENU_MENUITEM_SIGNAL_ITEM_ACTIVATED, G_CALLBACK(activate_cb), "indicator-datetime-preferences");
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

	/* Set up GSettings */
	conf = g_settings_new(SETTINGS_INTERFACE);
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

	mainloop = g_main_loop_new(NULL, FALSE);
	g_main_loop_run(mainloop);

	g_object_unref(G_OBJECT(conf));
	g_object_unref(G_OBJECT(master));
	g_object_unref(G_OBJECT(dbus));
	g_object_unref(G_OBJECT(service));
	g_object_unref(G_OBJECT(server));
	g_object_unref(G_OBJECT(root));

	geo_address_clean();
	geo_client_clean();

	return 0;
}
