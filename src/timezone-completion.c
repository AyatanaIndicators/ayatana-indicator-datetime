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

#include <json-glib/json-glib.h>
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
  GtkTreeModel * initial_model;
  GtkEntry *     entry;
  guint          queued_request;
  guint          changed_id;
  GCancellable * cancel;
  gchar *        request_text;
  GHashTable *   request_table;
};

#define TIMEZONE_COMPLETION_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE((o), TIMEZONE_COMPLETION_TYPE, TimezoneCompletionPrivate))

#define GEONAME_URL "http://geoname-lookup.ubuntu.com/?query=%s&release=%s"

/* Prototypes */
static void timezone_completion_class_init (TimezoneCompletionClass *klass);
static void timezone_completion_init       (TimezoneCompletion *self);
static void timezone_completion_dispose    (GObject *object);
static void timezone_completion_finalize   (GObject *object);

G_DEFINE_TYPE (TimezoneCompletion, timezone_completion, GTK_TYPE_ENTRY_COMPLETION);

static void
save_and_use_model (TimezoneCompletion * completion, GtkTreeModel * model)
{
  TimezoneCompletionPrivate * priv = TIMEZONE_COMPLETION_GET_PRIVATE(completion);

  g_hash_table_insert (priv->request_table, g_strdup (priv->request_text), g_object_ref (model));
  gtk_entry_completion_set_model (GTK_ENTRY_COMPLETION (completion), model);
  gtk_entry_completion_complete (GTK_ENTRY_COMPLETION (completion));
}

static void
json_parse_ready (GObject *object, GAsyncResult *res, gpointer user_data)
{
  TimezoneCompletion * completion = TIMEZONE_COMPLETION (user_data);
  TimezoneCompletionPrivate * priv = TIMEZONE_COMPLETION_GET_PRIVATE(completion);
  GError * error = NULL;

  json_parser_load_from_stream_finish (JSON_PARSER (object), res, &error);

  if (priv->cancel && (error == NULL || error->code != G_IO_ERROR_CANCELLED)) {
    g_cancellable_reset (priv->cancel);
  }

  if (error != NULL) {
    g_warning ("Could not parse geoname JSON data: %s", error->message);
    g_error_free (error);
    save_and_use_model (completion, priv->initial_model);
    return;
  }

  GtkListStore * store = gtk_list_store_new (TIMEZONE_COMPLETION_LAST,
                                             G_TYPE_STRING,
                                             G_TYPE_STRING,
                                             G_TYPE_STRING,
                                             G_TYPE_STRING,
                                             G_TYPE_STRING,
                                             G_TYPE_STRING);

  JsonReader * reader = json_reader_new (json_parser_get_root (JSON_PARSER (object)));

  if (!json_reader_is_array (reader))
    return;

  gint i, count = json_reader_count_elements (reader);
  for (i = 0; i < count; ++i) {
    if (!json_reader_read_element (reader, i))
      continue;

    if (json_reader_is_object (reader)) {
      const gchar * name = NULL;
      const gchar * admin1 = NULL;
      const gchar * country = NULL;
      const gchar * longitude = NULL;
      const gchar * latitude = NULL;
      if (json_reader_read_member (reader, "name")) {
        name = json_reader_get_string_value (reader);
        json_reader_end_member (reader);
      }
      if (json_reader_read_member (reader, "admin1")) {
        admin1 = json_reader_get_string_value (reader);
        json_reader_end_member (reader);
      }
      if (json_reader_read_member (reader, "country")) {
        country = json_reader_get_string_value (reader);
        json_reader_end_member (reader);
      }
      if (json_reader_read_member (reader, "longitude")) {
        longitude = json_reader_get_string_value (reader);
        json_reader_end_member (reader);
      }
      if (json_reader_read_member (reader, "latitude")) {
        latitude = json_reader_get_string_value (reader);
        json_reader_end_member (reader);
      }

      GtkTreeIter iter;
      gtk_list_store_append (store, &iter);
      gtk_list_store_set (store, &iter,
                          TIMEZONE_COMPLETION_ZONE, NULL,
                          TIMEZONE_COMPLETION_NAME, name,
                          TIMEZONE_COMPLETION_ADMIN1, admin1,
                          TIMEZONE_COMPLETION_COUNTRY, country,
                          TIMEZONE_COMPLETION_LONGITUDE, longitude,
                          TIMEZONE_COMPLETION_LATITUDE, latitude,
                          -1);
    }

    json_reader_end_element (reader);
  }

  save_and_use_model (completion, GTK_TREE_MODEL (store));
  g_object_unref (G_OBJECT (store));
}

static void
geonames_data_ready (GObject *object, GAsyncResult *res, gpointer user_data)
{
  TimezoneCompletion * completion = TIMEZONE_COMPLETION (user_data);
  TimezoneCompletionPrivate * priv = TIMEZONE_COMPLETION_GET_PRIVATE (completion);
  GError * error = NULL;
  GFileInputStream * stream;

  stream = g_file_read_finish (G_FILE (object), res, &error);

  if (priv->cancel && (error == NULL || error->code != G_IO_ERROR_CANCELLED)) {
    g_cancellable_reset (priv->cancel);
  }

  if (error != NULL) {
    g_warning ("Could not connect to geoname lookup server: %s", error->message);
    g_error_free (error);
    save_and_use_model (completion, priv->initial_model);
    return;
  }

  JsonParser * parser = json_parser_new ();
  json_parser_load_from_stream_async (parser, G_INPUT_STREAM (stream), priv->cancel,
                                      json_parse_ready, user_data);
}

static gboolean
request_zones (TimezoneCompletion * completion)
{
  TimezoneCompletionPrivate * priv = TIMEZONE_COMPLETION_GET_PRIVATE (completion);

  priv->queued_request = 0;

  if (priv->entry == NULL) {
    return FALSE;
  }

  const gchar * text = gtk_entry_get_text (priv->entry);

  gpointer data;
  if (g_hash_table_lookup_extended (priv->request_table, text, NULL, &data)) {
    gtk_entry_completion_set_model (GTK_ENTRY_COMPLETION (completion), GTK_TREE_MODEL (data));
    gtk_entry_completion_complete (GTK_ENTRY_COMPLETION (completion));
    return FALSE;
  }

  /* Cancel any ongoing request */
  if (priv->cancel) {
    g_cancellable_cancel (priv->cancel);
    g_cancellable_reset (priv->cancel);
  }
  g_free (priv->request_text);

  priv->request_text = g_strdup (text);

  gchar * escaped = g_uri_escape_string (text, NULL, FALSE);
  gchar * url = g_strdup_printf (GEONAME_URL, escaped, "11.04"); // FIXME: don't hardcode

  GFile * file =  g_file_new_for_uri (url);
  g_file_read_async (file, G_PRIORITY_DEFAULT, priv->cancel,
                     geonames_data_ready, completion);

  return FALSE;
}

static void
entry_changed (GtkEntry * entry, TimezoneCompletion * completion)
{
  TimezoneCompletionPrivate * priv = TIMEZONE_COMPLETION_GET_PRIVATE (completion);

  if (priv->queued_request) {
    g_source_remove (priv->queued_request);
  }
  priv->queued_request = g_timeout_add (300, (GSourceFunc)request_zones, completion);
}

void
timezone_completion_watch_entry (TimezoneCompletion * completion, GtkEntry * entry)
{
  TimezoneCompletionPrivate * priv = TIMEZONE_COMPLETION_GET_PRIVATE (completion);

  if (priv->entry) {
    g_source_remove (priv->changed_id);
    g_object_remove_weak_pointer (G_OBJECT (priv->entry), (gpointer *)&priv->entry);
  }

  guint id = g_signal_connect (entry, "changed", G_CALLBACK (entry_changed), completion);
  priv->changed_id = id;

  priv->entry = entry;
  g_object_add_weak_pointer (G_OBJECT (entry), (gpointer *)&priv->entry);
}

static GtkListStore *
get_initial_model (void)
{
  TzDB * db = tz_load_db ();
  GPtrArray * locations = tz_get_locations (db);

  GtkListStore * store = gtk_list_store_new (TIMEZONE_COMPLETION_LAST,
                                             G_TYPE_STRING,
                                             G_TYPE_STRING,
                                             G_TYPE_STRING,
                                             G_TYPE_STRING,
                                             G_TYPE_STRING,
                                             G_TYPE_STRING);

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
data_func (GtkCellLayout *cell_layout, GtkCellRenderer *cell,
           GtkTreeModel *tree_model, GtkTreeIter *iter, gpointer user_data)
{
  GValue name_val = {0}, admin1_val = {0}, country_val = {0};
  const gchar * name, * admin1, * country;

  gtk_tree_model_get_value (GTK_TREE_MODEL (tree_model), iter, TIMEZONE_COMPLETION_NAME, &name_val);
  gtk_tree_model_get_value (GTK_TREE_MODEL (tree_model), iter, TIMEZONE_COMPLETION_ADMIN1, &admin1_val);
  gtk_tree_model_get_value (GTK_TREE_MODEL (tree_model), iter, TIMEZONE_COMPLETION_COUNTRY, &country_val);

  name = g_value_get_string (&name_val);
  admin1 = g_value_get_string (&admin1_val);
  country = g_value_get_string (&country_val);

  gchar * user_name;
  if (admin1 == NULL || admin1[0] == 0) {
    user_name = g_strdup_printf ("%s <small>(%s)</small>", name, country);
  } else {
    user_name = g_strdup_printf ("%s <small>(%s, %s)</small>", name, admin1, country);
  }

  g_object_set (G_OBJECT (cell), "markup", user_name, NULL);

  g_value_unset (&name_val);
  g_value_unset (&admin1_val);
  g_value_unset (&country_val);
}

static gboolean
match_func (GtkEntryCompletion *completion, const gchar *key,
            GtkTreeIter *iter, gpointer user_data)
{
  // geonames does the work for us
  return TRUE;
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
timezone_completion_init (TimezoneCompletion * self)
{
  TimezoneCompletionPrivate * priv = TIMEZONE_COMPLETION_GET_PRIVATE (self);

  priv->initial_model = GTK_TREE_MODEL (get_initial_model ());

  gtk_entry_completion_set_match_func (GTK_ENTRY_COMPLETION (self), match_func, NULL, NULL);
  g_object_set (G_OBJECT (self),
                "text-column", TIMEZONE_COMPLETION_NAME,
                "popup-set-width", FALSE,
                NULL);

  priv->cancel = g_cancellable_new ();

  priv->request_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);

  GtkCellRenderer * cell = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (self), cell, TRUE);
  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (self), cell, data_func, NULL, NULL);

  return;
}

static void
timezone_completion_dispose (GObject * object)
{
  G_OBJECT_CLASS (timezone_completion_parent_class)->dispose (object);

  TimezoneCompletion * completion = TIMEZONE_COMPLETION (object);
  TimezoneCompletionPrivate * priv = TIMEZONE_COMPLETION_GET_PRIVATE (completion);

  if (priv->changed_id) {
    g_source_remove (priv->changed_id);
    priv->changed_id = 0;
  }

  if (priv->entry != NULL) {
    g_object_remove_weak_pointer (G_OBJECT (priv->entry), (gpointer *)&priv->entry);
  }

  if (priv->initial_model != NULL) {
    g_object_unref (G_OBJECT (priv->initial_model));
    priv->initial_model = NULL;
  }

  if (priv->queued_request) {
    g_source_remove (priv->queued_request);
    priv->queued_request = 0;
  }

  if (priv->cancel != NULL) {
    g_cancellable_cancel (priv->cancel);
    g_object_unref (priv->cancel);
    priv->cancel = NULL;
  }

  if (priv->request_text != NULL) {
    g_free (priv->request_text);
    priv->request_text = NULL;
  }

  if (priv->request_table != NULL) {
    g_hash_table_destroy (priv->request_table);
    priv->request_table = NULL;
  }

  return;
}

static void
timezone_completion_finalize (GObject * object)
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

