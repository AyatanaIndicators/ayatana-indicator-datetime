
#include <config.h>
#include <libindicator/indicator-service.h>

#include <glib/gi18n.h>

#include <libdbusmenu-glib/server.h>
#include <libdbusmenu-glib/client.h>
#include <libdbusmenu-glib/menuitem.h>

#include "dbus-shared.h"

static IndicatorService * service = NULL;
static GMainLoop * mainloop = NULL;
static DbusmenuServer * server = NULL;
static DbusmenuMenuitem * root = NULL;

/* Items */
static DbusmenuMenuitem * date = NULL;
static DbusmenuMenuitem * calendar = NULL;

/* Does the work to build the default menu, really calls out
   to other functions but this is the core to clean up the
   main function. */
static void
build_menus (DbusmenuMenuitem * root)
{
	if (date == NULL) {
		date = dbusmenu_menuitem_new();
		dbusmenu_menuitem_property_set     (date, DBUSMENU_MENUITEM_PROP_LABEL, _("No date yet..."));
		dbusmenu_menuitem_property_set_bool(date, DBUSMENU_MENUITEM_PROP_ENABLED, FALSE);
		dbusmenu_menuitem_child_append(root, date);
		//update_label(self);
	}

	if (calendar == NULL) {
		calendar = dbusmenu_menuitem_new();
		dbusmenu_menuitem_property_set     (calendar, DBUSMENU_MENUITEM_PROP_LABEL, _("Open Calendar"));
		/* insensitive until we check for available apps */
		dbusmenu_menuitem_property_set_bool(calendar, DBUSMENU_MENUITEM_PROP_ENABLED, FALSE);
		dbusmenu_menuitem_child_append(root, calendar);
		// queue checking for apps
	}

	DbusmenuMenuitem * separator = dbusmenu_menuitem_new();
	dbusmenu_menuitem_property_set(separator, DBUSMENU_MENUITEM_PROP_TYPE, DBUSMENU_CLIENT_TYPES_SEPARATOR);
	dbusmenu_menuitem_child_append(root, separator);

	DbusmenuMenuitem * settings = dbusmenu_menuitem_new();
	dbusmenu_menuitem_property_set     (settings, DBUSMENU_MENUITEM_PROP_LABEL, _("Set Time and Date..."));
	/* insensitive until we check for available apps */
	dbusmenu_menuitem_property_set_bool(settings, DBUSMENU_MENUITEM_PROP_ENABLED, FALSE);
	//g_signal_connect(G_OBJECT(settings), DBUSMENU_MENUITEM_SIGNAL_ACTIVATE, G_CALLBACK(activate_cb), "time-admin");
	dbusmenu_menuitem_child_append(root, settings);

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

	/* Building the base menu */
	server = dbusmenu_server_new(MENU_OBJ);
	root = dbusmenu_menuitem_new();
	dbusmenu_server_set_root(server, root);
	build_menus(root);

	mainloop = g_main_loop_new(NULL, FALSE);
	g_main_loop_run(mainloop);

	g_object_unref(G_OBJECT(service));
	g_object_unref(G_OBJECT(server));
	g_object_unref(G_OBJECT(root));

	return 0;
}
