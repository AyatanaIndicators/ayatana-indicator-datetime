/* -*- Mode: C; coding: utf-8; indent-tabs-mode: nil; tab-width: 2 -*-

Copyright 2011 Canonical Ltd.

Authors:
    Michael Terry <michael.terry@canonical.com>

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

#include <gdk/gdk.h>
#include <glib/gi18n.h>
#include "timezone-completion.h"
#include "tz.h"

enum {
  LAST_SIGNAL
};

/* static guint signals[LAST_SIGNAL] = { }; */

typedef struct _TimezoneCompletionPrivate TimezoneCompletionPrivate;
struct _TimezoneCompletionPrivate
{
  void * placeholder;
};

#define TIMEZONE_COMPLETION_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TIMEZONE_COMPLETION_TYPE, TimezoneCompletionPrivate))

/* Prototypes */
static void timezone_completion_class_init (TimezoneCompletionClass *klass);
static void timezone_completion_init       (TimezoneCompletion *self);
static void timezone_completion_dispose    (GObject *object);
static void timezone_completion_finalize   (GObject *object);

G_DEFINE_TYPE (TimezoneCompletion, timezone_completion, GTK_TYPE_ENTRY_COMPLETION);

static GtkListStore *
get_initial_model (void)
{
  TzDB * db = tz_load_db ();
  GPtrArray * locations = tz_get_locations (db);

  GtkListStore * store = gtk_list_store_new (TIMEZONE_COMPLETION_LAST, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

  gint i;
  for (i = 0; i < locations->len; ++i) {
    TzLocation * loc = g_ptr_array_index (locations, i);
    GtkTreeIter iter;
    gtk_list_store_append (store, &iter);

    /* FIXME: need something better than below for non-English locales */
    const gchar * last_bit = ((const gchar *)strrchr (loc->zone, '/')) + 1;
    if (last_bit == NULL)
      last_bit = loc->zone;
    gchar * name = g_strdup (last_bit);
    gchar * underscore;
    while ((underscore = strchr (name, '_'))) {
       *underscore = ' ';
    }

    gtk_list_store_set (store, &iter,
                        TIMEZONE_COMPLETION_ZONE, loc->zone,
                        TIMEZONE_COMPLETION_NAME, name,
                        TIMEZONE_COMPLETION_COUNTRY, loc->country,
                        -1);

    g_free (name);
  }

  tz_db_free (db);
  return store;
}

static void
timezone_completion_class_init (TimezoneCompletionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (TimezoneCompletionPrivate));

  object_class->dispose = timezone_completion_dispose;
  object_class->finalize = timezone_completion_finalize;

  return;
}

static void
timezone_completion_init (TimezoneCompletion *self)
{
  GtkListStore * model = get_initial_model ();
  gtk_entry_completion_set_model (GTK_ENTRY_COMPLETION (self), GTK_TREE_MODEL (model));
  gtk_entry_completion_set_text_column (GTK_ENTRY_COMPLETION (self), TIMEZONE_COMPLETION_NAME);
  return;
}

static void
timezone_completion_dispose (GObject *object)
{
  G_OBJECT_CLASS (timezone_completion_parent_class)->dispose (object);
  return;
}

static void
timezone_completion_finalize (GObject *object)
{
  G_OBJECT_CLASS (timezone_completion_parent_class)->finalize (object);
  return;
}

TimezoneCompletion *
timezone_completion_new ()
{
  TimezoneCompletion * self = g_object_new (TIMEZONE_COMPLETION_TYPE, NULL);
  return self;
}

