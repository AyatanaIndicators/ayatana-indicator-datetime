#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* GStuff */
#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n.h>

/* Indicator Stuff */
#include <libindicator/indicator.h>
#include <libindicator/indicator-object.h>


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
	GtkMenuItem * date;
	guint timer;
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
	self->priv->date = NULL;
	self->priv->timer = 0;

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

	if (self->priv->date != NULL) {
		g_object_unref(self->priv->date);
		self->priv->date = NULL;
	}

	if (self->priv->timer != 0) {
		g_source_remove(self->priv->timer);
		self->priv->timer = 0;
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

	strftime(longstr, 128, "%I:%M %p", ltime);
	
	gchar * utf8 = g_locale_to_utf8(longstr, -1, NULL, NULL, NULL);
	gtk_label_set_label(self->priv->label, utf8);
	g_free(utf8);

	if (self->priv->date == NULL) return;

	/* Note: may require some localization tweaks */
	strftime(longstr, 128, "%A, %e %B %Y", ltime);
	
	utf8 = g_locale_to_utf8(longstr, -1, NULL, NULL, NULL);
	gtk_menu_item_set_label(self->priv->date, utf8);
	g_free(utf8);

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

static void
settings_cb (GtkWidget *widget, gpointer dummy)
{
	GError * error = NULL;
	gchar *prgname = "time-admin";

	if (!g_spawn_command_line_async(prgname, &error)) {
		g_warning("Unable to start %s: %s", (char *)prgname, error->message);
		g_error_free(error);
	}
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
		update_label(self);
		gtk_widget_show(GTK_WIDGET(self->priv->label));
	}

	if (self->priv->timer == 0) {
		self->priv->timer = g_timeout_add_seconds(60, minute_timer_func, self);
	}

	return self->priv->label;
}

/* Build a dummy menu for now */
static GtkMenu *
get_menu (IndicatorObject * io)
{
	IndicatorDatetime * self = INDICATOR_DATETIME(io);

	GtkWidget * menu = NULL;
	GtkWidget * item = NULL;

	menu = gtk_menu_new();
	
	if (self->priv->date == NULL) {
		item = gtk_menu_item_new_with_label("No date yet...");
		gtk_widget_set_sensitive (GTK_WIDGET (item), FALSE);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
		self->priv->date = GTK_MENU_ITEM (item);
		update_label(self);
	}

	gtk_menu_shell_append (GTK_MENU_SHELL (menu),
						   gtk_separator_menu_item_new ());

	GtkWidget *settings_mi = gtk_menu_item_new_with_label (_("Set Time and Date..."));
	g_signal_connect (GTK_MENU_ITEM (settings_mi), "activate",
					  G_CALLBACK (settings_cb), NULL);
	gtk_menu_shell_append (GTK_MENU_SHELL(menu), settings_mi);

	gtk_widget_show_all(menu);

	return GTK_MENU(menu);
}
