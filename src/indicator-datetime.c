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

#define DEFAULT_TIME_12_FORMAT   "%l:%M %p"
#define DEFAULT_TIME_24_FORMAT   "%H:%M"
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
static void update_label                  (IndicatorDatetime * io);
static void guess_label_size              (IndicatorDatetime * self);

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

	self->priv->time_string = g_strdup(DEFAULT_TIME_FORMAT);

	self->priv->time_mode = SETTINGS_TIME_LOCALE;
	self->priv->show_seconds = FALSE;
	self->priv->show_date = FALSE;
	self->priv->show_day = FALSE;
	self->priv->custom_string = g_strdup(DEFAULT_TIME_FORMAT);

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
static void
update_label (IndicatorDatetime * io)
{
	IndicatorDatetime * self = INDICATOR_DATETIME(io);

	if (self->priv->label == NULL) return;

	gchar longstr[128];
	time_t t;
	struct tm *ltime;

	t = time(NULL);
	ltime = localtime(&t);
	if (ltime == NULL) {
		g_debug("Error getting local time");
		gtk_label_set_label(self->priv->label, _("Error getting time"));
		return;
	}

	strftime(longstr, 128, self->priv->time_string, ltime);
	
	gchar * utf8 = g_locale_to_utf8(longstr, -1, NULL, NULL, NULL);
	gtk_label_set_label(self->priv->label, utf8);
	g_free(utf8);

	if (self->priv->idle_measure == 0) {
		self->priv->idle_measure = g_idle_add(idle_measure, io);
	}

	return;
}

/* Runs every minute and updates the time */
gboolean
minute_timer_func (gpointer user_data)
{
	IndicatorDatetime * self = INDICATOR_DATETIME(user_data);

	if (self->priv->label != NULL) {
		update_label(self);
		return TRUE;
	} else {
		self->priv->timer = 0;
		return FALSE;
	}

	return FALSE;
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

#define FAT_NUMBER 8

/* Try to get a good guess at what a maximum width of the entire
   string would be. */
static void
guess_label_size (IndicatorDatetime * self)
{
	GtkStyle * style = gtk_widget_get_style(GTK_WIDGET(self->priv->label));
	PangoContext * context = gtk_widget_get_pango_context(GTK_WIDGET(self->priv->label));

	/* TRANSLATORS: This string is used for measuring the size of
	   the font used for showing the time and is not shown to the
	   user anywhere. */
	gchar * am_str = g_strdup_printf(_("%d%d:%d%d AM"), FAT_NUMBER, FAT_NUMBER, FAT_NUMBER, FAT_NUMBER);
	gint am_width = measure_string(style, context, am_str);
	g_free(am_str);

	/* TRANSLATORS: This string is used for measuring the size of
	   the font used for showing the time and is not shown to the
	   user anywhere. */
	gchar * pm_str = g_strdup_printf(_("%d%d:%d%d PM"), FAT_NUMBER, FAT_NUMBER, FAT_NUMBER, FAT_NUMBER);
	gint pm_width = measure_string(style, context, pm_str);
	g_free(pm_str);

	self->priv->max_width = MAX(am_width, pm_width);
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
		/* TRANSLATORS: This string is used to determine the default
		   clock style for your locale.  If it is the string '12' then
		   the default will be a 12-hour clock using AM/PM string.  If
		   it is '24' then it will be a 24-hour clock.  Users may over
		   ride this setting so it's still important to translate the
		   other strings no matter how this is set. */
		const gchar * locale_default = _("12");

		if (g_strcmp0(locale_default, "24") == 0) {
			twelvehour = FALSE;
		}
	} else if (self->priv->time_mode == SETTINGS_TIME_24_HOUR) {
		twelvehour = FALSE;
	}

	const gchar * time_string = NULL;
	if (twelvehour) {
		if (self->priv->show_seconds) {
			/* TRANSLATORS: A format string for the strftime function for
			   a clock showing 12-hour time with seconds. */
			time_string = _("%l:%M:%S %p");
		} else {
			/* TRANSLATORS: A format string for the strftime function for
			   a clock showing 12-hour time. */
			time_string = _(DEFAULT_TIME_12_FORMAT);
		}
	} else {
		if (self->priv->show_seconds) {
			/* TRANSLATORS: A format string for the strftime function for
			   a clock showing 24-hour time with seconds. */
			time_string = _("%H:%M:%S");
		} else {
			/* TRANSLATORS: A format string for the strftime function for
			   a clock showing 24-hour time. */
			time_string = _(DEFAULT_TIME_24_FORMAT);
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
		date_string = _("%a %b %e");
	} else if (self->priv->show_date) {
		/* TRANSLATORS:  This is a format string passed to strftime to represent
		   the month and the day of the month. */
		date_string = _("%b %e");
	} else if (self->priv->show_day) {
		/* TRANSLATORS:  This is a format string passed to strftime to represent
		   the day of the week. */
		date_string = _("%a");
	}

	/* Check point, we should have a date string */
	g_return_val_if_fail(date_string != NULL, g_strdup(time_string));

	/* TRANSLATORS: This is a format string passed to strftime to combine the
	   date and the time.  The value of "%s, %s" would result in a string like
	   this in US English 12-hour time: 'Fri Jul 16, 11:50 AM' */
	return g_strdup_printf(_("%s, %s"), date_string, time_string);
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
		self->priv->timer = g_timeout_add_seconds(60, minute_timer_func, self);
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

	return GTK_MENU(self->priv->menu);
}
