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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <locale.h>
#include <langinfo.h>
#include <string.h>

/* GStuff */
#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>
#include <gio/gio.h>

/* Indicator Stuff */
#include <libindicator/indicator.h>
#include <libindicator/indicator-object.h>
#include <libindicator/indicator-service-manager.h>

/* DBusMenu */
#include <libdbusmenu-gtk/menu.h>
#include <libido/idocalendarmenuitem.h>

#include "dbus-shared.h"


#define INDICATOR_DATETIME_TYPE            (indicator_datetime_get_type ())
#define INDICATOR_DATETIME(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), INDICATOR_DATETIME_TYPE, IndicatorDatetime))
#define INDICATOR_DATETIME_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), INDICATOR_DATETIME_TYPE, IndicatorDatetimeClass))
#define IS_INDICATOR_DATETIME(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), INDICATOR_DATETIME_TYPE))
#define IS_INDICATOR_DATETIME_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), INDICATOR_DATETIME_TYPE))
#define INDICATOR_DATETIME_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), INDICATOR_DATETIME_TYPE, IndicatorDatetimeClass))

typedef struct _IndicatorDatetime         IndicatorDatetime;
typedef struct _IndicatorDatetimeClass    IndicatorDatetimeClass;
typedef struct _IndicatorDatetimePrivate  IndicatorDatetimePrivate;

struct _IndicatorDatetimeClass {
	IndicatorObjectClass parent_class;
};

struct _IndicatorDatetime {
	IndicatorObject parent;
	IndicatorDatetimePrivate * priv;
};

struct _IndicatorDatetimePrivate {
	GtkLabel * label;
	guint timer;

	gchar * time_string;

	gint time_mode;
	gboolean show_seconds;
	gboolean show_date;
	gboolean show_day;
	gchar * custom_string;

	guint idle_measure;
	gint  max_width;

	IndicatorServiceManager * sm;
	DbusmenuGtkMenu * menu;

	GCancellable * service_proxy_cancel;
	GDBusProxy * service_proxy;
	IdoCalendarMenuItem *ido_calendar;

	GSettings * settings;
};

/* Enum for the properties so that they can be quickly
   found and looked up. */
enum {
	PROP_0,
	PROP_TIME_FORMAT,
	PROP_SHOW_SECONDS,
	PROP_SHOW_DAY,
	PROP_SHOW_DATE,
	PROP_CUSTOM_TIME_FORMAT
};

#define PROP_TIME_FORMAT_S              "time-format"
#define PROP_SHOW_SECONDS_S             "show-seconds"
#define PROP_SHOW_DAY_S                 "show-day"
#define PROP_SHOW_DATE_S                "show-date"
#define PROP_CUSTOM_TIME_FORMAT_S       "custom-time-format"

#define SETTINGS_INTERFACE              "org.ayatana.indicator.datetime"
#define SETTINGS_TIME_FORMAT_S          "time-format"
#define SETTINGS_SHOW_SECONDS_S         "show-seconds"
#define SETTINGS_SHOW_DAY_S             "show-day"
#define SETTINGS_SHOW_DATE_S            "show-date"
#define SETTINGS_CUSTOM_TIME_FORMAT_S   "custom-time-format"

enum {
	SETTINGS_TIME_LOCALE = 0,
	SETTINGS_TIME_12_HOUR = 1,
	SETTINGS_TIME_24_HOUR = 2,
	SETTINGS_TIME_CUSTOM = 3
};

/* TRANSLATORS: A format string for the strftime function for
   a clock showing 12-hour time without seconds. */
#define DEFAULT_TIME_12_FORMAT   N_("%l:%M %p")

/* TRANSLATORS: A format string for the strftime function for
   a clock showing 24-hour time without seconds. */
#define DEFAULT_TIME_24_FORMAT   N_("%H:%M")

#define DEFAULT_TIME_FORMAT      DEFAULT_TIME_12_FORMAT

#define INDICATOR_DATETIME_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE ((o), INDICATOR_DATETIME_TYPE, IndicatorDatetimePrivate))

GType indicator_datetime_get_type (void);

static void indicator_datetime_class_init (IndicatorDatetimeClass *klass);
static void indicator_datetime_init       (IndicatorDatetime *self);
static void set_property (GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec);
static void get_property (GObject * object, guint prop_id, GValue * value, GParamSpec * pspec);
static void indicator_datetime_dispose    (GObject *object);
static void indicator_datetime_finalize   (GObject *object);
static GtkLabel * get_label               (IndicatorObject * io);
static GtkMenu *  get_menu                (IndicatorObject * io);
static GVariant * bind_enum_set           (const GValue * value, const GVariantType * type, gpointer user_data);
static gboolean bind_enum_get             (GValue * value, GVariant * variant, gpointer user_data);
static gchar * generate_format_string     (IndicatorDatetime * self);
static struct tm * update_label           (IndicatorDatetime * io);
static void guess_label_size              (IndicatorDatetime * self);
static void setup_timer                   (IndicatorDatetime * self, struct tm * ltime);
static void update_time                   (IndicatorDatetime * self);
static void receive_signal                (GDBusProxy * proxy, gchar * sender_name, gchar * signal_name, GVariant parameters, gpointer user_data);
static void service_proxy_cb (GObject * object, GAsyncResult * res, gpointer user_data);

/* Indicator Module Config */
INDICATOR_SET_VERSION
INDICATOR_SET_TYPE(INDICATOR_DATETIME_TYPE)

G_DEFINE_TYPE (IndicatorDatetime, indicator_datetime, INDICATOR_OBJECT_TYPE);

static void
indicator_datetime_class_init (IndicatorDatetimeClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (IndicatorDatetimePrivate));

	object_class->dispose = indicator_datetime_dispose;
	object_class->finalize = indicator_datetime_finalize;

	object_class->set_property = set_property;
	object_class->get_property = get_property;

	IndicatorObjectClass * io_class = INDICATOR_OBJECT_CLASS(klass);

	io_class->get_label = get_label;
	io_class->get_menu  = get_menu;

	g_object_class_install_property (object_class,
	                                 PROP_TIME_FORMAT,
	                                 g_param_spec_int(PROP_TIME_FORMAT_S,
	                                                  "A choice of which format should be used on the panel",
	                                                  "Chooses between letting the locale choose the time, 12-hour time, 24-time or using the custom string passed to strftime().",
	                                                  SETTINGS_TIME_LOCALE, /* min */
	                                                  SETTINGS_TIME_CUSTOM, /* max */
	                                                  SETTINGS_TIME_LOCALE, /* default */
	                                                  G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (object_class,
	                                 PROP_SHOW_SECONDS,
	                                 g_param_spec_boolean(PROP_SHOW_SECONDS_S,
	                                                      "Whether to show seconds in the indicator.",
	                                                      "Shows seconds along with the time in the indicator.  Also effects refresh interval.",
	                                                      FALSE, /* default */
	                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (object_class,
	                                 PROP_SHOW_DAY,
	                                 g_param_spec_boolean(PROP_SHOW_DAY_S,
	                                                      "Whether to show the day of the week in the indicator.",
	                                                      "Shows the day of the week along with the time in the indicator.",
	                                                      FALSE, /* default */
	                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (object_class,
	                                 PROP_SHOW_DATE,
	                                 g_param_spec_boolean(PROP_SHOW_DATE_S,
	                                                      "Whether to show the day and month in the indicator.",
	                                                      "Shows the day and month along with the time in the indicator.",
	                                                      FALSE, /* default */
	                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (object_class,
	                                 PROP_CUSTOM_TIME_FORMAT,
	                                 g_param_spec_string(PROP_CUSTOM_TIME_FORMAT_S,
	                                                     "The format that is used to show the time on the panel.",
	                                                     "A format string in the form used to pass to strftime to make a string for displaying on the panel.",
	                                                     DEFAULT_TIME_FORMAT,
	                                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	return;
}

static void
indicator_datetime_init (IndicatorDatetime *self)
{
	self->priv = INDICATOR_DATETIME_GET_PRIVATE(self);

	self->priv->label = NULL;
	self->priv->timer = 0;

	self->priv->idle_measure = 0;
	self->priv->max_width = 0;

	self->priv->time_mode = SETTINGS_TIME_LOCALE;
	self->priv->show_seconds = FALSE;
	self->priv->show_date = FALSE;
	self->priv->show_day = FALSE;
	self->priv->custom_string = g_strdup(DEFAULT_TIME_FORMAT);

	self->priv->time_string = generate_format_string(self);

	self->priv->service_proxy = NULL;

	self->priv->sm = NULL;
	self->priv->menu = NULL;

	self->priv->settings = g_settings_new(SETTINGS_INTERFACE);
	if (self->priv->settings != NULL) {
		g_settings_bind_with_mapping(self->priv->settings,
		                SETTINGS_TIME_FORMAT_S,
		                self,
		                PROP_TIME_FORMAT_S,
		                G_SETTINGS_BIND_DEFAULT,
		                bind_enum_get,
		                bind_enum_set,
		                NULL, NULL); /* Userdata and destroy func */
		g_settings_bind(self->priv->settings,
		                SETTINGS_SHOW_SECONDS_S,
		                self,
		                PROP_SHOW_SECONDS_S,
		                G_SETTINGS_BIND_DEFAULT);
		g_settings_bind(self->priv->settings,
		                SETTINGS_SHOW_DAY_S,
		                self,
		                PROP_SHOW_DAY_S,
		                G_SETTINGS_BIND_DEFAULT);
		g_settings_bind(self->priv->settings,
		                SETTINGS_SHOW_DATE_S,
		                self,
		                PROP_SHOW_DATE_S,
		                G_SETTINGS_BIND_DEFAULT);
		g_settings_bind(self->priv->settings,
		                SETTINGS_CUSTOM_TIME_FORMAT_S,
		                self,
		                PROP_CUSTOM_TIME_FORMAT_S,
		                G_SETTINGS_BIND_DEFAULT);
	} else {
		g_warning("Unable to get settings for '" SETTINGS_INTERFACE "'");
	}

	self->priv->sm = indicator_service_manager_new_version(SERVICE_NAME, SERVICE_VERSION);

	self->priv->service_proxy_cancel = g_cancellable_new();

	g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
		                  G_DBUS_PROXY_FLAGS_NONE,
		                  NULL,
		                  SERVICE_NAME,
		                  SERVICE_OBJ,
		                  SERVICE_IFACE,
		                  self->priv->service_proxy_cancel,
		                  service_proxy_cb,
                                  self);

	return;
}

/* Callback from trying to create the proxy for the serivce, this
   could include starting the service.  Sometime it'll fail and
   we'll try to start that dang service again! */
static void
service_proxy_cb (GObject * object, GAsyncResult * res, gpointer user_data)
{
	GError * error = NULL;

	IndicatorDatetime * self = INDICATOR_DATETIME(user_data);
	g_return_if_fail(self != NULL);

	GDBusProxy * proxy = g_dbus_proxy_new_for_bus_finish(res, &error);

	IndicatorDatetimePrivate * priv = INDICATOR_DATETIME_GET_PRIVATE(self);

	if (priv->service_proxy_cancel != NULL) {
		g_object_unref(priv->service_proxy_cancel);
		priv->service_proxy_cancel = NULL;
	}

	if (error != NULL) {
		g_warning("Could not grab DBus proxy for %s: %s", SERVICE_NAME, error->message);
		g_error_free(error);
		return;
	}

	/* Okay, we're good to grab the proxy at this point, we're
	sure that it's ours. */
	priv->service_proxy = proxy;

	g_signal_connect(proxy, "g-signal", G_CALLBACK(receive_signal), self);

	return;
}

static void
indicator_datetime_dispose (GObject *object)
{
	IndicatorDatetime * self = INDICATOR_DATETIME(object);

	if (self->priv->label != NULL) {
		g_object_unref(self->priv->label);
		self->priv->label = NULL;
	}

	if (self->priv->timer != 0) {
		g_source_remove(self->priv->timer);
		self->priv->timer = 0;
	}

	if (self->priv->idle_measure != 0) {
		g_source_remove(self->priv->idle_measure);
		self->priv->idle_measure = 0;
	}

	if (self->priv->menu != NULL) {
		g_object_unref(G_OBJECT(self->priv->menu));
		self->priv->menu = NULL;
	}

	if (self->priv->sm != NULL) {
		g_object_unref(G_OBJECT(self->priv->sm));
		self->priv->sm = NULL;
	}

	if (self->priv->settings != NULL) {
		g_object_unref(G_OBJECT(self->priv->settings));
		self->priv->settings = NULL;
	}

	if (self->priv->service_proxy != NULL) {
		g_object_unref(self->priv->service_proxy);
		self->priv->service_proxy = NULL;
	}

	G_OBJECT_CLASS (indicator_datetime_parent_class)->dispose (object);
	return;
}

static void
indicator_datetime_finalize (GObject *object)
{
	IndicatorDatetime * self = INDICATOR_DATETIME(object);

	if (self->priv->time_string != NULL) {
		g_free(self->priv->time_string);
		self->priv->time_string = NULL;
	}

	if (self->priv->custom_string != NULL) {
		g_free(self->priv->custom_string);
		self->priv->custom_string = NULL;
	}

	G_OBJECT_CLASS (indicator_datetime_parent_class)->finalize (object);
	return;
}

/* Turns the int value into a string GVariant */
static GVariant *
bind_enum_set (const GValue * value, const GVariantType * type, gpointer user_data)
{
	switch (g_value_get_int(value)) {
	case SETTINGS_TIME_LOCALE:
		return g_variant_new_string("locale-default");
	case SETTINGS_TIME_12_HOUR:
		return g_variant_new_string("12-hour");
	case SETTINGS_TIME_24_HOUR:
		return g_variant_new_string("24-hour");
	case SETTINGS_TIME_CUSTOM:
		return g_variant_new_string("custom");
	default:
		return NULL;
	}
}

/* Turns a string GVariant into an int value */
static gboolean
bind_enum_get (GValue * value, GVariant * variant, gpointer user_data)
{
	const gchar * str = g_variant_get_string(variant, NULL);
	gint output = 0;

	if (g_strcmp0(str, "locale-default") == 0) {
		output = SETTINGS_TIME_LOCALE;
	} else if (g_strcmp0(str, "12-hour") == 0) {
		output = SETTINGS_TIME_12_HOUR;
	} else if (g_strcmp0(str, "24-hour") == 0) {
		output = SETTINGS_TIME_24_HOUR;
	} else if (g_strcmp0(str, "custom") == 0) {
		output = SETTINGS_TIME_CUSTOM;
	} else {
		return FALSE;
	}

	g_value_set_int(value, output);
	return TRUE;
}

/* Sets a property on the object */
static void
set_property (GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec)
{
	IndicatorDatetime * self = INDICATOR_DATETIME(object);
	gboolean update = FALSE;

	switch(prop_id) {
	case PROP_TIME_FORMAT: {
		gint newval = g_value_get_int(value);
		if (newval != self->priv->time_mode) {
			update = TRUE;
			self->priv->time_mode = newval;
		}
		break;
	}
	case PROP_SHOW_SECONDS:
		if (g_value_get_boolean(value) != self->priv->show_seconds) {
			self->priv->show_seconds = !self->priv->show_seconds;
			if (self->priv->time_mode != SETTINGS_TIME_CUSTOM) {
				update = TRUE;
				setup_timer(self, NULL);
			}
		}
		break;
	case PROP_SHOW_DAY:
		if (g_value_get_boolean(value) != self->priv->show_day) {
			self->priv->show_day = !self->priv->show_day;
			if (self->priv->time_mode != SETTINGS_TIME_CUSTOM) {
				update = TRUE;
			}
		}
		break;
	case PROP_SHOW_DATE:
		if (g_value_get_boolean(value) != self->priv->show_date) {
			self->priv->show_date = !self->priv->show_date;
			if (self->priv->time_mode != SETTINGS_TIME_CUSTOM) {
				update = TRUE;
			}
		}
		break;
	case PROP_CUSTOM_TIME_FORMAT: {
		const gchar * newstr = g_value_get_string(value);
		if (g_strcmp0(newstr, self->priv->custom_string) != 0) {
			if (self->priv->custom_string != NULL) {
				g_free(self->priv->custom_string);
				self->priv->custom_string = NULL;
			}
			self->priv->custom_string = g_strdup(newstr);
			if (self->priv->time_mode == SETTINGS_TIME_CUSTOM) {
				update = TRUE;
				setup_timer(self, NULL);
			}
		}
		break;
	}
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		return;
	}

	if (!update) {
		return;
	}

	/* Get the new format string */
	gchar * newformat = generate_format_string(self);

	/* check to ensure the format really changed */
	if (g_strcmp0(self->priv->time_string, newformat) == 0) {
		g_free(newformat);
		return;
	}

	/* Okay now process the change */
	if (self->priv->time_string != NULL) {
		g_free(self->priv->time_string);
		self->priv->time_string = NULL;
	}
	self->priv->time_string = newformat;

	/* And update everything */
	update_label(self);
	guess_label_size(self);

	return;
}

/* Gets a property from the object */
static void
get_property (GObject * object, guint prop_id, GValue * value, GParamSpec * pspec)
{
	IndicatorDatetime * self = INDICATOR_DATETIME(object);

	switch(prop_id) {
	case PROP_TIME_FORMAT:
		g_value_set_int(value, self->priv->time_mode);
		break;
	case PROP_SHOW_SECONDS:
		g_value_set_boolean(value, self->priv->show_seconds);
		break;
	case PROP_SHOW_DAY:
		g_value_set_boolean(value, self->priv->show_day);
		break;
	case PROP_SHOW_DATE:
		g_value_set_boolean(value, self->priv->show_date);
		break;
	case PROP_CUSTOM_TIME_FORMAT:
		g_value_set_string(value, self->priv->custom_string);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		return;
	}

	return;
}

/* Looks at the size of the label, if it grew beyond what we
   thought was the max, make sure it doesn't shrink again. */
static gboolean
idle_measure (gpointer data)
{
	IndicatorDatetime * self = INDICATOR_DATETIME(data);
	self->priv->idle_measure = 0;

	GtkAllocation allocation;
	gtk_widget_get_allocation(GTK_WIDGET(self->priv->label), &allocation);

	if (allocation.width > self->priv->max_width) {
		if (self->priv->max_width != 0) {
			g_warning("Guessed wrong.  We thought the max would be %d but we're now at %d", self->priv->max_width, allocation.width);
		}
		self->priv->max_width = allocation.width;
		gtk_widget_set_size_request(GTK_WIDGET(self->priv->label), self->priv->max_width, -1);
	}

	return FALSE;
}

/* Updates the label to be the current time. */
static struct tm *
update_label (IndicatorDatetime * io)
{
	IndicatorDatetime * self = INDICATOR_DATETIME(io);

	if (self->priv->label == NULL) return NULL;

	gchar longstr[128];
	time_t t;
	struct tm *ltime;

	t = time(NULL);
	ltime = localtime(&t);
	if (ltime == NULL) {
		g_debug("Error getting local time");
		gtk_label_set_label(self->priv->label, _("Error getting time"));
		return NULL;
	}

	strftime(longstr, 128, self->priv->time_string, ltime);
	
	gchar * utf8 = g_locale_to_utf8(longstr, -1, NULL, NULL, NULL);
	gtk_label_set_label(self->priv->label, utf8);
	g_free(utf8);

	if (self->priv->idle_measure == 0) {
		self->priv->idle_measure = g_idle_add(idle_measure, io);
	}

	return ltime;
}

/* Update the time right now.  Usually the result of a timezone switch. */
static void
update_time (IndicatorDatetime * self)
{
	struct tm * ltime = update_label(self);
	setup_timer(self, ltime);
	return;
}

/* Receives all signals from the service, routed to the appropriate functions */
static void
receive_signal (GDBusProxy * proxy, gchar * sender_name, gchar * signal_name,
                GVariant parameters, gpointer user_data)
{
	IndicatorDatetime * self = INDICATOR_DATETIME(user_data);

	if (g_strcmp0(signal_name, "UpdateTime") == 0) {
		update_time(self);
	}

	return;
}

/* Runs every minute and updates the time */
gboolean
timer_func (gpointer user_data)
{
	IndicatorDatetime * self = INDICATOR_DATETIME(user_data);
	self->priv->timer = 0;
	struct tm * ltime = update_label(self);
	setup_timer(self, ltime);
	return FALSE;
}

/* Configure the timer to run the next time through */
static void
setup_timer (IndicatorDatetime * self, struct tm * ltime)
{
	if (self->priv->timer != 0) {
		g_source_remove(self->priv->timer);
		self->priv->timer = 0;
	}
	
	if (self->priv->show_seconds) {
		self->priv->timer = g_timeout_add_seconds(1, timer_func, self);
	} else {
		if (ltime == NULL) {
			time_t t;
			t = time(NULL);
			ltime = localtime(&t);
		}

		/* Plus 2 so we're just after the minute, don't want to be early. */
		self->priv->timer = g_timeout_add_seconds(60 - ltime->tm_sec + 2, timer_func, self);
	}

	return;
}

/* Does a quick meausre of how big the string is in
   pixels with a Pango layout */
static gint
measure_string (GtkStyle * style, PangoContext * context, const gchar * string)
{
	PangoLayout * layout = pango_layout_new(context);
	pango_layout_set_text(layout, string, -1);
	pango_layout_set_font_description(layout, style->font_desc);

	gint width;
	pango_layout_get_pixel_size(layout, &width, NULL);
	g_object_unref(layout);
	return width;
}

/* Format for the table of strftime() modifiers to what
   we need to check when determining the length */
typedef struct _strftime_type_t strftime_type_t;
struct _strftime_type_t {
	char character;
	gint mask;
};

enum {
	STRFTIME_MASK_NONE    = 0,      /* Hours or minutes as we always test those */
	STRFTIME_MASK_SECONDS = 1 << 0, /* Seconds count */
	STRFTIME_MASK_AMPM    = 1 << 1, /* AM/PM counts */
	STRFTIME_MASK_WEEK    = 1 << 2, /* Day of the week maters (Sat, Sun, etc.) */
	STRFTIME_MASK_DAY     = 1 << 3, /* Day of the month counts (Feb 1st) */
	STRFTIME_MASK_MONTH   = 1 << 4, /* Which month matters */
	STRFTIME_MASK_YEAR    = 1 << 5, /* Which year matters */
	/* Last entry, combines all previous */
	STRFTIME_MASK_ALL     = (STRFTIME_MASK_SECONDS | STRFTIME_MASK_AMPM | STRFTIME_MASK_WEEK | STRFTIME_MASK_DAY | STRFTIME_MASK_MONTH | STRFTIME_MASK_YEAR)
};

/* A table taken from the man page of strftime to what the different
   characters can effect.  These are worst case in that we need to
   test the length based on all these things to ensure that we have
   a reasonable string lenght measurement. */
const static strftime_type_t strftime_type[] = {
	{'a', STRFTIME_MASK_WEEK},
	{'A', STRFTIME_MASK_WEEK},
	{'b', STRFTIME_MASK_MONTH},
	{'B', STRFTIME_MASK_MONTH},
	{'c', STRFTIME_MASK_ALL}, /* We don't know, so we have to assume all */
	{'C', STRFTIME_MASK_YEAR},
	{'d', STRFTIME_MASK_MONTH},
	{'D', STRFTIME_MASK_MONTH | STRFTIME_MASK_YEAR | STRFTIME_MASK_DAY},
	{'e', STRFTIME_MASK_DAY},
	{'F', STRFTIME_MASK_MONTH | STRFTIME_MASK_YEAR | STRFTIME_MASK_DAY},
	{'G', STRFTIME_MASK_YEAR},
	{'g', STRFTIME_MASK_YEAR},
	{'h', STRFTIME_MASK_MONTH},
	{'j', STRFTIME_MASK_DAY},
	{'m', STRFTIME_MASK_MONTH},
	{'p', STRFTIME_MASK_AMPM},
	{'P', STRFTIME_MASK_AMPM},
	{'r', STRFTIME_MASK_AMPM},
	{'s', STRFTIME_MASK_SECONDS},
	{'S', STRFTIME_MASK_SECONDS},
	{'T', STRFTIME_MASK_SECONDS},
	{'u', STRFTIME_MASK_WEEK},
	{'U', STRFTIME_MASK_DAY | STRFTIME_MASK_MONTH},
	{'V', STRFTIME_MASK_DAY | STRFTIME_MASK_MONTH},
	{'w', STRFTIME_MASK_DAY},
	{'W', STRFTIME_MASK_DAY | STRFTIME_MASK_MONTH},
	{'x', STRFTIME_MASK_YEAR | STRFTIME_MASK_MONTH | STRFTIME_MASK_DAY | STRFTIME_MASK_WEEK},
	{'X', STRFTIME_MASK_SECONDS},
	{'y', STRFTIME_MASK_YEAR},
	{'Y', STRFTIME_MASK_YEAR},
	/* Last one */
	{0, 0}
};

#define FAT_NUMBER 8

/* Looks through the characters in the format string to
   ensure that we can figure out which of the things we
   need to check in determining the length. */
static gint
generate_strftime_bitmask (IndicatorDatetime * self)
{
	gint retval = 0;
	glong strlength = g_utf8_strlen(self->priv->time_string, -1);
	gint i;
	g_debug("Evaluating bitmask for '%s'", self->priv->time_string);

	for (i = 0; i < strlength; i++) {
		if (self->priv->time_string[i] == '%' && i + 1 < strlength) {
			gchar evalchar = self->priv->time_string[i + 1];

			/* If we're using alternate formats we need to skip those characters */
			if (evalchar == 'E' || evalchar == 'O') {
				if (i + 2 < strlength) {
					evalchar = self->priv->time_string[i + 2];
				} else {
					continue;
				}
			}

			/* Let's look at that character in the table */
			int j;
			for (j = 0; strftime_type[j].character != 0; j++) {
				if (strftime_type[j].character == evalchar) {
					retval |= strftime_type[j].mask;
					break;
				}
			}
		}
	}

	return retval;
}

/* Build an array up of all the time values that we want to check
   for length to ensure we're in a good place */
static void
build_timeval_array (GArray * timevals, gint mask)
{
	struct tm mytm = {0};

	/* Sun 12/28/8888 00:00 */
	mytm.tm_hour = 0;
	mytm.tm_mday = 28;
	mytm.tm_mon = 11;
	mytm.tm_year = 8888 - 1900;
	mytm.tm_wday = 0;
	mytm.tm_yday = 363;
	g_array_append_val(timevals, mytm);

	if (mask & STRFTIME_MASK_AMPM) {
		/* Sun 12/28/8888 12:00 */
		mytm.tm_hour = 12;
		g_array_append_val(timevals, mytm);
	}

	/* NOTE: Ignoring year 8888 should handle it */

	if (mask & STRFTIME_MASK_MONTH) {
		gint oldlen = timevals->len;
		gint i, j;
		for (i = 0; i < oldlen; i++) {
			for (j = 0; j < 11; j++) {
				struct tm localval = g_array_index(timevals, struct tm, i);
				localval.tm_mon = j;
				/* Not sure if I need to adjust yday & wday, hope not */
				g_array_append_val(timevals, localval);
			}
		}
	}

	/* Doing these together as it seems like just slightly more
	   coverage on the numerical days, but worth it. */
	if (mask & (STRFTIME_MASK_WEEK | STRFTIME_MASK_DAY)) {
		gint oldlen = timevals->len;
		gint i, j;
		for (i = 0; i < oldlen; i++) {
			for (j = 22; j < 28; j++) {
				struct tm localval = g_array_index(timevals, struct tm, i);

				gint diff = 28 - j;

				localval.tm_mday = j;
				localval.tm_wday = localval.tm_wday - diff;
				if (localval.tm_wday < 0) {
					localval.tm_wday += 7;
				}
				localval.tm_yday = localval.tm_yday - diff;

				g_array_append_val(timevals, localval);
			}
		}
	}

	return;
}

/* Try to get a good guess at what a maximum width of the entire
   string would be. */
static void
guess_label_size (IndicatorDatetime * self)
{
	/* This is during startup. */
	if (self->priv->label == NULL) return;

	GtkStyle * style = gtk_widget_get_style(GTK_WIDGET(self->priv->label));
	PangoContext * context = gtk_widget_get_pango_context(GTK_WIDGET(self->priv->label));
	gint * max_width = &(self->priv->max_width);
	gint posibilitymask = generate_strftime_bitmask(self);

	/* Build the array of possibilities that we want to test */
	GArray * timevals = g_array_new(FALSE, TRUE, sizeof(struct tm));
	build_timeval_array(timevals, posibilitymask);

	g_debug("Checking against %d posible times", timevals->len);
	gint check_time;
	for (check_time = 0; check_time < timevals->len; check_time++) {
		gchar longstr[128];
		strftime(longstr, 128, self->priv->time_string, &(g_array_index(timevals, struct tm, check_time)));
		
		gchar * utf8 = g_locale_to_utf8(longstr, -1, NULL, NULL, NULL);
		gint length = measure_string(style, context, utf8);
		g_free(utf8);

		if (length > *max_width) {
			*max_width = length;
		}
	}

	g_array_free(timevals, TRUE);

	gtk_widget_set_size_request(GTK_WIDGET(self->priv->label), self->priv->max_width, -1);
	g_debug("Guessing max time width: %d", self->priv->max_width);

	return;
}

/* React to the style changing, which could mean an font
   update. */
static void
style_changed (GtkWidget * widget, GtkStyle * oldstyle, gpointer data)
{
	g_debug("New style for time label");
	IndicatorDatetime * self = INDICATOR_DATETIME(data);
	guess_label_size(self);
	update_label(self);
	return;
}

/* Translate msg according to the locale specified by LC_TIME */
static char *
T_(const char *msg)
{
	/* General strategy here is to make sure LANGUAGE is empty (since that
	   trumps all LC_* vars) and then to temporarily swap LC_TIME and
	   LC_MESSAGES.  Then have gettext translate msg.

	   We strdup the strings because the setlocale & *env functions do not
	   guarantee anything about the storage used for the string, and thus
	   the string may not be portably safe after multiple calls.

	   Note that while you might think g_dcgettext would do the trick here,
	   that actually looks in /usr/share/locale/XX/LC_TIME, not the
	   LC_MESSAGES directory, so we won't find any translation there.
	*/
	char *message_locale = g_strdup(setlocale(LC_MESSAGES, NULL));
	char *time_locale = g_strdup(setlocale(LC_TIME, NULL));
	char *language = g_strdup(g_getenv("LANGUAGE"));
	char *rv;
	g_unsetenv("LANGUAGE");
	setlocale(LC_MESSAGES, time_locale);

	/* Get the LC_TIME version */
	rv = _(msg);

	/* Put everything back the way it was */
	setlocale(LC_MESSAGES, message_locale);
	g_setenv("LANGUAGE", language, TRUE);
	g_free(message_locale);
	g_free(time_locale);
	g_free(language);
	return rv;
}

/* Check the system locale setting to see if the format is 24-hour
   time or 12-hour time */
static gboolean
is_locale_12h()
{
	static const char *formats_24h[] = {"%H", "%R", "%T", "%OH", "%k", NULL};
	const char *t_fmt = nl_langinfo(T_FMT);
	int i;

	for (i = 0; formats_24h[i]; ++i) {
		if (strstr(t_fmt, formats_24h[i])) {
			return FALSE;
		}
	}

	return TRUE;
}

/* Tries to figure out what our format string should be.  Lots
   of translator comments in here. */
static gchar *
generate_format_string (IndicatorDatetime * self)
{
	if (self->priv->time_mode == SETTINGS_TIME_CUSTOM) {
		return g_strdup(self->priv->custom_string);
	}

	gboolean twelvehour = TRUE;

	if (self->priv->time_mode == SETTINGS_TIME_LOCALE) {
		twelvehour = is_locale_12h();
	} else if (self->priv->time_mode == SETTINGS_TIME_24_HOUR) {
		twelvehour = FALSE;
	}

	const gchar * time_string = NULL;
	if (twelvehour) {
		if (self->priv->show_seconds) {
			/* TRANSLATORS: A format string for the strftime function for
			   a clock showing 12-hour time with seconds. */
			time_string = T_("%l:%M:%S %p");
		} else {
			time_string = T_(DEFAULT_TIME_12_FORMAT);
		}
	} else {
		if (self->priv->show_seconds) {
			/* TRANSLATORS: A format string for the strftime function for
			   a clock showing 24-hour time with seconds. */
			time_string = T_("%H:%M:%S");
		} else {
			time_string = T_(DEFAULT_TIME_24_FORMAT);
		}
	}
	
	/* Checkpoint, let's not fail */
	g_return_val_if_fail(time_string != NULL, g_strdup(DEFAULT_TIME_FORMAT));

	/* If there's no date or day let's just leave now and
	   not worry about the rest of this code */
	if (!self->priv->show_date && !self->priv->show_day) {
		return g_strdup(time_string);
	}

	const gchar * date_string = NULL;
	if (self->priv->show_date && self->priv->show_day) {
		/* TRANSLATORS:  This is a format string passed to strftime to represent
		   the day of the week, the month and the day of the month. */
		date_string = T_("%a %b %e");
	} else if (self->priv->show_date) {
		/* TRANSLATORS:  This is a format string passed to strftime to represent
		   the month and the day of the month. */
		date_string = T_("%b %e");
	} else if (self->priv->show_day) {
		/* TRANSLATORS:  This is a format string passed to strftime to represent
		   the day of the week. */
		date_string = T_("%a");
	}

	/* Check point, we should have a date string */
	g_return_val_if_fail(date_string != NULL, g_strdup(time_string));

	/* TRANSLATORS: This is a format string passed to strftime to combine the
	   date and the time.  The value of "%s, %s" would result in a string like
	   this in US English 12-hour time: 'Fri Jul 16, 11:50 AM' */
	return g_strdup_printf(T_("%s, %s"), date_string, time_string);
}

static gboolean
new_calendar_item (DbusmenuMenuitem * newitem,
				   DbusmenuMenuitem * parent,
				   DbusmenuClient   * client)
{
	g_return_val_if_fail(DBUSMENU_IS_MENUITEM(newitem), FALSE);
	g_return_val_if_fail(DBUSMENU_IS_GTKCLIENT(client), FALSE);
	/* Note: not checking parent, it's reasonable for it to be NULL */

	IndicatorObject *io = g_object_get_data (G_OBJECT (client), "indicator");
	if (io == NULL) {
		g_warning ("found no indicator to attach the caledar to");
		return FALSE;
	}

	IndicatorDatetime *self = INDICATOR_DATETIME(io);
	self->priv = INDICATOR_DATETIME_GET_PRIVATE(self);
	
	IdoCalendarMenuItem *ido = IDO_CALENDAR_MENU_ITEM (ido_calendar_menu_item_new ());
	self->priv->ido_calendar = ido;

	dbusmenu_gtkclient_newitem_base(DBUSMENU_GTKCLIENT(client), newitem, GTK_MENU_ITEM(ido), parent);

	return TRUE;
}

/* Grabs the label.  Creates it if it doesn't
   exist already */
static GtkLabel *
get_label (IndicatorObject * io)
{
	IndicatorDatetime * self = INDICATOR_DATETIME(io);

	/* If there's not a label, we'll build ourselves one */
	if (self->priv->label == NULL) {
		self->priv->label = GTK_LABEL(gtk_label_new("Time"));
		g_object_ref(G_OBJECT(self->priv->label));
		g_signal_connect(G_OBJECT(self->priv->label), "style-set", G_CALLBACK(style_changed), self);
		guess_label_size(self);
		update_label(self);
		gtk_widget_show(GTK_WIDGET(self->priv->label));
	}

	if (self->priv->timer == 0) {
		setup_timer(self, NULL);
	}

	return self->priv->label;
}

static GtkMenu *
get_menu (IndicatorObject * io)
{
	IndicatorDatetime * self = INDICATOR_DATETIME(io);

	if (self->priv->menu == NULL) {
		self->priv->menu = dbusmenu_gtkmenu_new(SERVICE_NAME, MENU_OBJ);
	}

	DbusmenuGtkClient *client = dbusmenu_gtkmenu_get_client(self->priv->menu);
	g_object_set_data (G_OBJECT (client), "indicator", io);

	dbusmenu_client_add_type_handler(DBUSMENU_CLIENT(client), DBUSMENU_CALENDAR_MENUITEM_TYPE, new_calendar_item);

	return GTK_MENU(self->priv->menu);
}
