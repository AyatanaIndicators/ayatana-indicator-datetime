/* -*- Mode: C; coding: utf-8; indent-tabs-mode: nil; tab-width: 2 -*-

A dialog for setting time and date preferences.

Copyright 2010 Canonical Ltd.

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

#ifndef INDICATOR_DATETIME_UTILS_H
#define INDICATOR_DATETIME_UTILS_H

#include <glib.h>
#include <gio/gio.h> /* GSettings */

G_BEGIN_DECLS

gboolean      is_locale_12h                        (void);

void          split_settings_location              (const char  * location,
                                                    char       ** zone,
                                                    char       ** name);

gchar *       get_current_zone_name                (const char  * location,
                                                    GSettings   * settings);

gchar *       generate_full_format_string_at_time  (GDateTime   * now,
                                                    GDateTime   * time);
  
G_END_DECLS

#endif /* INDICATOR_DATETIME_UTILS_H */
