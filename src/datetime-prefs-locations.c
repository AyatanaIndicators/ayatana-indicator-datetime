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

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "datetime-prefs-locations.h"
#include "settings-shared.h"

#define DATETIME_DIALOG_UI_FILE PKGDATADIR "/datetime-dialog.ui"

static void
handle_add (GtkWidget * button, gpointer user_data)
{
}

static void
handle_remove (GtkWidget * button, gpointer user_data)
{
}

static void
fill_from_settings (GObject * store, GSettings * conf)
{
  gchar ** locations = g_settings_get_strv (conf, SETTINGS_LOCATIONS_S);

  gtk_list_store_clear (GTK_LIST_STORE (store));

  gchar ** striter;
  GtkTreeIter iter;
  for (striter = locations; *striter; ++striter) {
    gtk_list_store_append (GTK_LIST_STORE (store), &iter);
    gtk_list_store_set (GTK_LIST_STORE (store), &iter, 0, *striter, -1);
  }

  g_strfreev (locations);
}

static void
save_to_settings (GtkWidget * dlg, GObject * store)
{
  GVariantBuilder builder;
  g_variant_builder_init (&builder, G_VARIANT_TYPE_ARRAY);

  GtkTreeIter iter;
  if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter)) {
    do {
      GValue value = {0};
      gtk_tree_model_get_value (GTK_TREE_MODEL (store), &iter, 0, &value);
      g_variant_builder_add (&builder, "s", g_value_get_string (&value));
      g_value_unset (&value);
    } while (gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter));
  }

  GVariant * locations = g_variant_builder_end (&builder);

  GSettings * conf = G_SETTINGS (g_object_get_data (G_OBJECT (dlg), "conf"));
  g_settings_set_strv (conf, SETTINGS_LOCATIONS_S, g_variant_get_strv (locations, NULL));

  g_variant_unref (locations);
}

GtkWidget *
datetime_setup_locations_dialog (GtkWindow * parent)
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
  GtkCellRenderer * cell = gtk_cell_renderer_text_new ();
  g_object_set (cell, "editable", TRUE, NULL);
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree), -1,
                                               _("Location"), cell,
                                               "text", 0, NULL);
  cell = gtk_cell_renderer_text_new ();
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree), -1,
                                               _("Time"), cell,
                                               "text", 1, NULL);
  gtk_tree_selection_set_mode (gtk_tree_view_get_selection (GTK_TREE_VIEW (tree)), GTK_SELECTION_MULTIPLE);

  g_signal_connect (WIG ("addButton"), "clicked", G_CALLBACK (handle_add), NULL);
  g_signal_connect (WIG ("removeButton"), "clicked", G_CALLBACK (handle_remove), NULL);

  fill_from_settings (store, conf);
  g_object_set_data_full (G_OBJECT (dlg), "conf", g_object_ref (conf), g_object_unref);
  g_signal_connect (dlg, "destroy", G_CALLBACK (save_to_settings), store);

  gtk_window_set_transient_for (GTK_WINDOW (dlg), parent);

#undef WIG

  g_object_unref (conf);
  g_object_unref (builder);

  return dlg;
}

