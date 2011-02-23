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
    return;
  }

g_print("got json\n");
  GtkListStore * store = GTK_LIST_STORE (gtk_entry_completion_get_model (GTK_ENTRY_COMPLETION (completion)));
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

g_print("adding %s\n", name);
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

  g_hash_table_insert (priv->request_table, g_strdup (priv->request_text), NULL);
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

g_print("requesting json?\n");
  if (priv->entry == NULL) {
    return FALSE;
  }

  const gchar * text = gtk_entry_get_text (priv->entry);

  if (g_hash_table_lookup_extended (priv->request_table, text, NULL, NULL))
    return FALSE; // already looked this up

  /* Cancel any ongoing request */
  if (priv->cancel) {
    g_cancellable_cancel (priv->cancel);
    g_cancellable_reset (priv->cancel);
  }
  g_free (priv->request_text);

  priv->request_text = g_strdup (text);

g_print("requesting json now\n");
  gchar * escaped = g_uri_escape_string (text, NULL, FALSE);
  gchar * url = g_strdup_printf (GEONAME_URL, escaped, "11.04");

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

  GtkListStore * model = get_initial_model ();
  gtk_entry_completion_set_model (GTK_ENTRY_COMPLETION (self), GTK_TREE_MODEL (model));
  gtk_entry_completion_set_text_column (GTK_ENTRY_COMPLETION (self), TIMEZONE_COMPLETION_NAME);

  priv->cancel = g_cancellable_new ();

  priv->request_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

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

  if (priv->queued_request) {
    g_source_remove (priv->queued_request);
    priv->queued_request = 0;
  }

  if (priv->cancel != NULL) {
    g_cancellable_cancel (priv->cancel);
    g_object_unref (priv->cancel);
    priv->cancel = NULL;
  }

  g_free (priv->request_text);
  g_hash_table_destroy (priv->request_table);

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

