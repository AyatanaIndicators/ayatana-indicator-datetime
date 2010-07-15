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

	guint idle_measure;
	gint  max_width;

	IndicatorServiceManager * sm;
	DbusmenuGtkMenu * menu;
};

#define INDICATOR_DATETIME_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE ((o), INDICATOR_DATETIME_TYPE, IndicatorDatetimePrivate))

GType indicator_datetime_get_type (void);

static void indicator_datetime_class_init (IndicatorDatetimeClass *klass);
static void indicator_datetime_init       (IndicatorDatetime *self);
static void indicator_datetime_dispose    (GObject *object);
static void indicator_datetime_finalize   (GObject *object);
static GtkLabel * get_label               (IndicatorObject * io);
static GtkMenu *  get_menu                (IndicatorObject * io);

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

	IndicatorObjectClass * io_class = INDICATOR_OBJECT_CLASS(klass);

	io_class->get_label = get_label;
	io_class->get_menu  = get_menu;

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

	self->priv->sm = NULL;
	self->priv->menu = NULL;

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

	G_OBJECT_CLASS (indicator_datetime_parent_class)->dispose (object);
	return;
}

static void
indicator_datetime_finalize (GObject *object)
{

	G_OBJECT_CLASS (indicator_datetime_parent_class)->finalize (object);
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

	strftime(longstr, 128, "%l:%M %p", ltime);
	
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
