/*
 * Copyright 2013 Canonical Ltd.
 *
 * Authors:
 *   Charles Kerr <charles.kerr@canonical.com>
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3, as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranties of
 * MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <gio/gio.h>

#include "timezone-file.h"

enum
{
  PROP_0,
  PROP_FILENAME,
  PROP_LAST
};

static GParamSpec * properties[PROP_LAST] = { 0 };

struct _IndicatorDatetimeTimezoneFilePriv
{
  gchar * filename;
  GFile * file;
  GFileMonitor * monitor;
  gchar * timezone;
};

typedef IndicatorDatetimeTimezoneFilePriv priv_t;

G_DEFINE_TYPE (IndicatorDatetimeTimezoneFile,
               indicator_datetime_timezone_file,
               INDICATOR_TYPE_DATETIME_TIMEZONE)

/***
****
***/

static void
reload (IndicatorDatetimeTimezoneFile * self)
{
  priv_t * p = self->priv;

  GError * err = NULL;
  gchar * new_timezone = NULL;

  if (!g_file_get_contents (p->filename, &new_timezone, NULL, &err))
    {
      g_warning ("%s Unable to read timezone file '%s': %s", G_STRLOC, p->filename, err->message);
      g_error_free (err);
    }
  else
    {
      g_strstrip (new_timezone);

      if (g_strcmp0 (p->timezone, new_timezone))
        {
          g_free (p->timezone);
          p->timezone = g_strdup (new_timezone);
          g_debug ("%s new timezone set: '%s'", G_STRLOC, p->timezone);
          indicator_datetime_timezone_notify_timezone (INDICATOR_DATETIME_TIMEZONE(self));
        }

      g_free (new_timezone);
    }
}

static void
set_filename (IndicatorDatetimeTimezoneFile * self, const char * filename)
{
  GError * err;
  priv_t * p = self->priv;

  g_clear_object (&p->monitor);
  g_clear_object (&p->file);
  g_free (p->filename);

  p->filename = g_strdup (filename);
  p->file = g_file_new_for_path (p->filename);

  err = NULL;
  p->monitor = g_file_monitor_file (p->file, G_FILE_MONITOR_NONE, NULL, &err);
  if (err != NULL)
    {
      g_warning ("%s Unable to monitor timezone file '%s': %s", G_STRLOC, TIMEZONE_FILE, err->message);
      g_error_free (err);
    }
  else
    {
      g_signal_connect_swapped (p->monitor, "changed", G_CALLBACK(reload), self);
      g_debug ("%s Monitoring timezone file '%s'", G_STRLOC, p->filename);
    }

  reload (self);
}

/***
**** IndicatorDatetimeTimezoneClass funcs
***/

static const char *
my_get_timezone (IndicatorDatetimeTimezone * self)
{
  return INDICATOR_DATETIME_TIMEZONE_FILE(self)->priv->timezone;
}

/***
**** GObjectClass funcs
***/

static void
my_get_property (GObject     * o,
                 guint         property_id,
                 GValue      * value,
                 GParamSpec  * pspec)
{
  IndicatorDatetimeTimezoneFile * self = INDICATOR_DATETIME_TIMEZONE_FILE (o);

  switch (property_id)
    {
      case PROP_FILENAME:
        g_value_set_string (value, self->priv->filename);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (o, property_id, pspec);
    }
}

static void
my_set_property (GObject       * o,
                 guint           property_id,
                 const GValue  * value,
                 GParamSpec    * pspec)
{
  IndicatorDatetimeTimezoneFile * self = INDICATOR_DATETIME_TIMEZONE_FILE (o);

  switch (property_id)
    {
      case PROP_FILENAME:
        set_filename (self, g_value_get_string (value));
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (o, property_id, pspec);
    }
}
  
static void
my_dispose (GObject * o)
{
  IndicatorDatetimeTimezoneFile * self = INDICATOR_DATETIME_TIMEZONE_FILE (o);
  priv_t * p = self->priv;

  g_clear_object (&p->monitor);
  g_clear_object (&p->file);

  G_OBJECT_CLASS (indicator_datetime_timezone_file_parent_class)->dispose (o);
}

static void
my_finalize (GObject * o)
{
  IndicatorDatetimeTimezoneFile * self = INDICATOR_DATETIME_TIMEZONE_FILE (o);
  priv_t * p = self->priv;

  g_free (p->filename);
  g_free (p->timezone);

  G_OBJECT_CLASS (indicator_datetime_timezone_file_parent_class)->finalize (o);
}

/***
****
***/

static void
indicator_datetime_timezone_file_class_init (IndicatorDatetimeTimezoneFileClass * klass)
{
  GObjectClass * object_class;
  IndicatorDatetimeTimezoneClass * location_class;
  const GParamFlags flags = G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS;

  object_class = G_OBJECT_CLASS (klass);
  object_class->dispose = my_dispose;
  object_class->finalize = my_finalize;
  object_class->set_property = my_set_property;
  object_class->get_property = my_get_property;

  location_class = INDICATOR_DATETIME_TIMEZONE_CLASS (klass);
  location_class->get_timezone = my_get_timezone;

  g_type_class_add_private (klass, sizeof (IndicatorDatetimeTimezoneFilePriv));

  /* install properties */

  properties[PROP_0] = NULL;

  properties[PROP_FILENAME] = g_param_spec_string ("filename",
                                                   "Filename",
                                                   "Filename to monitor for TZ changes",
                                                   "",
                                                   flags);

  g_object_class_install_properties (object_class, PROP_LAST, properties);
}

static void
indicator_datetime_timezone_file_init (IndicatorDatetimeTimezoneFile * self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            INDICATOR_TYPE_DATETIME_TIMEZONE_FILE,
                                            IndicatorDatetimeTimezoneFilePriv);
}

/***
****  Public
***/

IndicatorDatetimeTimezone *
indicator_datetime_timezone_file_new (const char * filename)
{
  gpointer o = g_object_new (INDICATOR_TYPE_DATETIME_TIMEZONE_FILE, "filename", filename, NULL);

  return INDICATOR_DATETIME_TIMEZONE (o);
}
