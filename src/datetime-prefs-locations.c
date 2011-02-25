/* -*- Mode: C; coding: utf-8; indent-tabs-mode: nil; tab-width: 2 -*-

A dialog for setting time and date preferences.

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

#include <stdlib.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include "datetime-prefs-locations.h"
#include "settings-shared.h"
#include "utils.h"
#include "timezone-completion.h"

#define DATETIME_DIALOG_UI_FILE PKGDATADIR "/datetime-dialog.ui"

static void
handle_add (GtkWidget * button, GtkTreeView * tree)
{
  GtkListStore * store = GTK_LIST_STORE (gtk_tree_view_get_model (tree));

  GtkTreeIter iter;
  gtk_list_store_append (store, &iter);

  GtkTreePath * path = gtk_tree_model_get_path (GTK_TREE_MODEL (store), &iter);
  gtk_tree_view_set_cursor (tree, path, gtk_tree_view_get_column (tree, 0), TRUE);
  gtk_tree_path_free (path);
}

static void
handle_remove (GtkWidget * button, GtkTreeView * tree)
{
  GtkListStore * store = GTK_LIST_STORE (gtk_tree_view_get_model (tree));
  GtkTreeSelection * selection = gtk_tree_view_get_selection (tree);

  GList * paths = gtk_tree_selection_get_selected_rows (selection, NULL);

  /* Convert all paths to iters so we can safely delete multiple paths.  For a
     GtkListStore, iters persist past model changes. */
  GList * tree_iters = NULL;
  GList * iter;
  for (iter = paths; iter; iter = iter->next) {
    GtkTreeIter * tree_iter = g_new(GtkTreeIter, 1);
    if (gtk_tree_model_get_iter (GTK_TREE_MODEL (store), tree_iter, (GtkTreePath *)iter->data)) {
      tree_iters = g_list_prepend (tree_iters, tree_iter);
    }
    gtk_tree_path_free (iter->data);
  }
  g_list_free (paths);

  /* Now delete each iterator */
  for (iter = tree_iters; iter; iter = iter->next) {
    gtk_list_store_remove (store, (GtkTreeIter *)iter->data);
    g_free (iter->data);
  }
  g_list_free (tree_iters);
}

static void
handle_edit (GtkCellRendererText * renderer, gchar * path, gchar * new_text,
             GtkListStore * store)
{
  GtkTreeIter iter;

  if (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (store), &iter, path)) {
    gtk_list_store_set (store, &iter, 0, new_text, -1);
  }
}

static gboolean
timezone_selected (GtkEntryCompletion * widget, GtkTreeModel * model,
                   GtkTreeIter * iter, gpointer user_data)
{
  GValue zone_value = {0}, name_value = {0};
  const gchar * zone, * name;

  gtk_tree_model_get_value (model, iter, TIMEZONE_COMPLETION_ZONE, &zone_value);
  zone = g_value_get_string (&zone_value);

  gtk_tree_model_get_value (model, iter, TIMEZONE_COMPLETION_NAME, &name_value);
  name = g_value_get_string (&name_value);

  if (zone == NULL || zone[0] == 0) {
    GValue lon_value = {0}, lat_value = {0};
    const gchar * strlon, * strlat;
    gdouble lon = 0.0, lat = 0.0;

    gtk_tree_model_get_value (model, iter, TIMEZONE_COMPLETION_LONGITUDE, &lon_value);
    strlon = g_value_get_string (&lon_value);
    if (strlon != NULL && strlon[0] != 0) {
      lon = strtod(strlon, NULL);
    }

    gtk_tree_model_get_value (model, iter, TIMEZONE_COMPLETION_LATITUDE, &lat_value);
    strlat = g_value_get_string (&lat_value);
    if (strlat != NULL && strlat[0] != 0) {
      lat = strtod(strlat, NULL);
    }

    CcTimezoneMap * tzmap = CC_TIMEZONE_MAP (g_object_get_data (G_OBJECT (widget), "tzmap"));
    zone = cc_timezone_map_get_timezone_at_coords (tzmap, lon, lat);
  }

  GtkListStore * store = GTK_LIST_STORE (g_object_get_data (G_OBJECT (widget), "store"));
  GtkTreeIter * store_iter = (GtkTreeIter *)g_object_get_data (G_OBJECT (widget), "store_iter");
  if (store != NULL && store_iter != NULL) {
    gtk_list_store_set (store, store_iter, 0, name, 2, zone, -1);
  }

  g_value_unset (&name_value);
  g_value_unset (&zone_value);

  return FALSE; // Do normal action too
}

static void
handle_edit_started (GtkCellRendererText * renderer, GtkCellEditable * editable,
                     gchar * path, TimezoneCompletion * completion)
{
  if (GTK_IS_ENTRY (editable)) {
    GtkEntry *entry = GTK_ENTRY (editable);
    gtk_entry_set_completion (entry, GTK_ENTRY_COMPLETION (completion));
    timezone_completion_watch_entry (completion, entry);

    GtkListStore * store = GTK_LIST_STORE (g_object_get_data (G_OBJECT (completion), "store"));
    GtkTreeIter * store_iter = g_new(GtkTreeIter, 1);
    if (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (store), store_iter, path)) {
      g_object_set_data_full (G_OBJECT (completion), "store_iter", store_iter, g_free);
    }
  }
}

static gboolean
update_times (TimezoneCompletion * completion)
{
  /* For each entry, check zone in column 2 and set column 1 to it's time */
  GtkListStore * store = GTK_LIST_STORE (g_object_get_data (G_OBJECT (completion), "store"));
  GObject * cell = G_OBJECT (g_object_get_data (G_OBJECT (completion), "name-cell"));

  gboolean editing;
  g_object_get (cell, "editing", &editing, NULL);
  if (editing) { /* No updates while editing, it cancels the edit */
    return TRUE;
  }

  GDateTime * now = g_date_time_new_now_local ();

  GtkTreeIter iter;
  if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter)) {
    do {
      GValue zone_value = {0};
      const gchar * strzone;

      gtk_tree_model_get_value (GTK_TREE_MODEL (store), &iter, 2, &zone_value);
      strzone = g_value_get_string (&zone_value);

      if (strzone != NULL && strzone[0] != 0) {
        GTimeZone * tz = g_time_zone_new (strzone);
        GDateTime * now_tz = g_date_time_to_timezone (now, tz);
        gchar * format = generate_format_string_at_time (now_tz);
        gchar * time_str = g_date_time_format (now_tz, format);

        gtk_list_store_set (store, &iter, 1, time_str, -1);

        g_free (time_str);
        g_free (format);
        g_date_time_unref (now_tz);
        g_time_zone_unref (tz);
      }

      g_value_unset (&zone_value);
    } while (gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter));
  }

  g_date_time_unref (now);
  return TRUE;
}

static void
fill_from_settings (GObject * store, GSettings * conf)
{
  gchar ** locations = g_settings_get_strv (conf, SETTINGS_LOCATIONS_S);

  gtk_list_store_clear (GTK_LIST_STORE (store));

  gchar ** striter;
  GtkTreeIter iter;
  for (striter = locations; *striter; ++striter) {
    gchar * zone, * name;
    split_settings_location (*striter, &zone, &name);

    gtk_list_store_append (GTK_LIST_STORE (store), &iter);
    gtk_list_store_set (GTK_LIST_STORE (store), &iter, 0, name, 2, zone, -1);

    g_free (zone);
    g_free (name);
  }

  g_strfreev (locations);
}

static void
dialog_closed (GtkWidget * dlg, GObject * store)
{
  /* Cleanup a tad */
  guint time_id = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (dlg), "time-id"));
  g_source_remove (time_id);

  /* Now save to settings */
  GVariantBuilder builder;
  g_variant_builder_init (&builder, G_VARIANT_TYPE_ARRAY);

  GtkTreeIter iter;
  if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter)) {
    do {
      GValue zone_value = {0}, name_value = {0};
      const gchar * strzone, * strname;

      gtk_tree_model_get_value (GTK_TREE_MODEL (store), &iter, 0, &name_value);
      gtk_tree_model_get_value (GTK_TREE_MODEL (store), &iter, 2, &zone_value);

      strzone = g_value_get_string (&zone_value);
      strname = g_value_get_string (&name_value);

      if (strzone != NULL && strzone[0] != 0 && strname != NULL && strname[0] != 0) {
        gchar * settings_string = g_strdup_printf("%s %s", strzone, strname);
        g_variant_builder_add (&builder, "s", settings_string);
        g_free (settings_string);
      }

      g_value_unset (&zone_value);
      g_value_unset (&name_value);
    } while (gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter));
  }

  GVariant * locations = g_variant_builder_end (&builder);

  GSettings * conf = G_SETTINGS (g_object_get_data (G_OBJECT (dlg), "conf"));
  g_settings_set_strv (conf, SETTINGS_LOCATIONS_S, g_variant_get_strv (locations, NULL));

  g_variant_unref (locations);
}

static void
selection_changed (GtkTreeSelection * selection, GtkWidget * remove_button)
{
  gint count = gtk_tree_selection_count_selected_rows (selection);
  gtk_widget_set_sensitive (remove_button, count > 0);
}

GtkWidget *
datetime_setup_locations_dialog (GtkWindow * parent, CcTimezoneMap * map)
{
  GError * error = NULL;
  GtkBuilder * builder = gtk_builder_new ();
  gtk_builder_add_from_file (builder, DATETIME_DIALOG_UI_FILE, &error);
  if (error != NULL) {
    /* We have to abort, we can't continue without the ui file */
    g_error ("Could not load ui file %s: %s", DATETIME_DIALOG_UI_FILE, error->message);
    g_error_free (error);
    return NULL;
  }

  gtk_builder_set_translation_domain (builder, GETTEXT_PACKAGE);

  GSettings * conf = g_settings_new (SETTINGS_INTERFACE);

#define WIG(name) GTK_WIDGET (gtk_builder_get_object (builder, name))

  GtkWidget * dlg = WIG ("locationsDialog");
  GtkWidget * tree = WIG ("locationsView");
  GObject * store = gtk_builder_get_object (builder, "locationsStore");

  /* Configure tree */
  TimezoneCompletion * completion = timezone_completion_new ();
  g_object_set_data (G_OBJECT (completion), "tzmap", map);
  g_object_set_data (G_OBJECT (completion), "store", store);
  g_signal_connect (completion, "match-selected", G_CALLBACK (timezone_selected), NULL);

  GtkCellRenderer * cell = gtk_cell_renderer_text_new ();
  g_object_set (cell, "editable", TRUE, NULL);
  g_signal_connect (cell, "editing-started", G_CALLBACK (handle_edit_started), completion);
  g_signal_connect (cell, "edited", G_CALLBACK (handle_edit), store);
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree), -1,
                                               _("Location"), cell,
                                               "text", 0, NULL);
  GtkTreeViewColumn * loc_col = gtk_tree_view_get_column (GTK_TREE_VIEW (tree), 0);
  gtk_tree_view_column_set_expand (loc_col, TRUE);
  g_object_set_data (G_OBJECT (completion), "name-cell", cell);

  cell = gtk_cell_renderer_text_new ();
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree), -1,
                                               _("Time"), cell,
                                               "text", 1, NULL);

  GtkTreeSelection * selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);
  g_signal_connect (selection, "changed", G_CALLBACK (selection_changed), WIG ("removeButton"));
  selection_changed (selection, WIG ("removeButton"));

  g_signal_connect (WIG ("addButton"), "clicked", G_CALLBACK (handle_add), tree);
  g_signal_connect (WIG ("removeButton"), "clicked", G_CALLBACK (handle_remove), tree);

  fill_from_settings (store, conf);

  guint time_id = g_timeout_add_seconds (2, (GSourceFunc)update_times, completion);
  update_times (completion);

  g_object_set_data_full (G_OBJECT (dlg), "conf", g_object_ref (conf), g_object_unref);
  g_object_set_data_full (G_OBJECT (dlg), "completion", completion, g_object_unref);
  g_object_set_data (G_OBJECT (dlg), "time-id", GINT_TO_POINTER(time_id));
  g_signal_connect (dlg, "destroy", G_CALLBACK (dialog_closed), store);

  gtk_window_set_transient_for (GTK_WINDOW (dlg), parent);

#undef WIG

  g_object_unref (conf);
  g_object_unref (builder);

  return dlg;
}

