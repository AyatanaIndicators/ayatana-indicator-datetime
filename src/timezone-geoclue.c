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

#include <geoclue/geoclue-master.h>
#include <geoclue/geoclue-master-client.h>

#include "timezone-geoclue.h"

struct _IndicatorDatetimeTimezoneGeocluePriv
{
  GeoclueMaster * master;
  GeoclueMasterClient * client;
  GeoclueAddress * address;
  gchar * timezone;
};

typedef IndicatorDatetimeTimezoneGeocluePriv priv_t;

G_DEFINE_TYPE (IndicatorDatetimeTimezoneGeoclue,
               indicator_datetime_timezone_geoclue,
               INDICATOR_TYPE_DATETIME_TIMEZONE)

static void geo_restart (IndicatorDatetimeTimezoneGeoclue * self);

/***
****
***/

static void
set_timezone (IndicatorDatetimeTimezoneGeoclue * self, const gchar * timezone)
{
  priv_t * p = self->priv;

  if (g_strcmp0 (p->timezone, timezone))
    {
      g_free (p->timezone);
      p->timezone = g_strdup (timezone);
      indicator_datetime_timezone_notify_timezone (INDICATOR_DATETIME_TIMEZONE(self));
    }
}

static void
on_address_changed (GeoclueAddress  * address,
                    int               timestamp,
                    GHashTable      * addy_data,
                    GeoclueAccuracy * accuracy,
                    GError          * error,
                    gpointer          gself)
{
  if (error != NULL)
    {
      g_warning ("%s Unable to get timezone from GeoClue: %s", G_STRFUNC, error->message);
      g_error_free (error);
    }
  else
    {
      IndicatorDatetimeTimezoneGeoclue * self = INDICATOR_DATETIME_TIMEZONE_GEOCLUE (gself);
      const char * timezone = g_hash_table_lookup (addy_data, "timezone");
      set_timezone (self, timezone);
    }
}

static void
on_address_created (GeoclueMasterClient * master,
                    GeoclueAddress      * address,
                    GError              * error,
                    gpointer              gself)
{
  if (error != NULL)
    {
      g_warning ("%s Unable to get timezone from GeoClue: %s", G_STRFUNC, error->message);
      g_error_free (error);
    }
  else
    {
      priv_t * p = INDICATOR_DATETIME_TIMEZONE_GEOCLUE(gself)->priv;

      g_assert (p->address == NULL);
      p->address = g_object_ref (address);

      geoclue_address_get_address_async (address, on_address_changed, gself);
      g_signal_connect (address, "address-changed", G_CALLBACK(on_address_changed), gself);
    }
}

static void
on_requirements_set (GeoclueMasterClient * master, GError * error, gpointer user_data)
{
  if (error != NULL)
    {
      g_warning ("%s Unable to get timezone from GeoClue: %s", G_STRFUNC, error->message);
      g_error_free (error);
    }
}

static void
on_client_created (GeoclueMaster        * master,
                   GeoclueMasterClient  * client,
                   gchar                * path,
                   GError               * error,
                   gpointer               gself)
{
  g_debug ("Created Geoclue client at: %s", path);

  if (error != NULL)
    {
      g_warning ("%s Unable to get timezone from GeoClue: %s", G_STRFUNC, error->message);
      g_error_free (error);
    }
  else if (client == NULL)
    {
      g_warning ("%s Unable to get timezone from GeoClue: %s", G_STRFUNC, error->message);
    }
  else
    {
      IndicatorDatetimeTimezoneGeoclue * self = INDICATOR_DATETIME_TIMEZONE_GEOCLUE (gself);
      priv_t * p = self->priv;

      g_clear_object (&p->client);
      p->client = g_object_ref (client);
      g_signal_connect_swapped (p->client, "invalidated", G_CALLBACK(geo_restart), gself);

      geoclue_master_client_set_requirements_async (p->client,
                                                    GEOCLUE_ACCURACY_LEVEL_REGION,
                                                    0,
                                                    FALSE,
                                                    GEOCLUE_RESOURCE_ALL,
                                                    on_requirements_set,
                                                    NULL);

      geoclue_master_client_create_address_async (p->client, on_address_created, gself);
    }
}

static void
geo_start (IndicatorDatetimeTimezoneGeoclue * self)
{
  priv_t * p = self->priv;

  g_assert (p->master == NULL);
  p->master = geoclue_master_get_default ();
  geoclue_master_create_client_async (p->master, on_client_created, self);
}

static void
geo_stop (IndicatorDatetimeTimezoneGeoclue * self)
{
  priv_t * p = self->priv;

  if (p->address != NULL)
    {
      g_signal_handlers_disconnect_by_func (p->address, on_address_changed, self);
      g_clear_object (&p->address);
    }

  if (p->client != NULL)
    {
      g_signal_handlers_disconnect_by_func (p->client, geo_restart, self);
      g_clear_object (&p->client);
    }

  g_clear_object (&p->master);
}

static void
geo_restart (IndicatorDatetimeTimezoneGeoclue * self)
{
  geo_stop (self);
  geo_start (self);
}

/***
****
***/

static const char *
my_get_timezone (IndicatorDatetimeTimezone * self)
{
  return INDICATOR_DATETIME_TIMEZONE_GEOCLUE(self)->priv->timezone;
}

static void
my_dispose (GObject * o)
{
  geo_stop (INDICATOR_DATETIME_TIMEZONE_GEOCLUE (o));

  G_OBJECT_CLASS (indicator_datetime_timezone_geoclue_parent_class)->dispose (o);
}

static void
my_finalize (GObject * o)
{
  IndicatorDatetimeTimezoneGeoclue * self = INDICATOR_DATETIME_TIMEZONE_GEOCLUE (o);
  priv_t * p = self->priv;

  g_free (p->timezone);

  G_OBJECT_CLASS (indicator_datetime_timezone_geoclue_parent_class)->finalize (o);
}

static void
indicator_datetime_timezone_geoclue_class_init (IndicatorDatetimeTimezoneGeoclueClass * klass)
{
  GObjectClass * object_class;
  IndicatorDatetimeTimezoneClass * location_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->dispose = my_dispose;
  object_class->finalize = my_finalize;

  location_class = INDICATOR_DATETIME_TIMEZONE_CLASS (klass);
  location_class->get_timezone = my_get_timezone;

  g_type_class_add_private (klass, sizeof (IndicatorDatetimeTimezoneGeocluePriv));
}

static void
indicator_datetime_timezone_geoclue_init (IndicatorDatetimeTimezoneGeoclue * self)
{
  priv_t * p;

  p = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                   INDICATOR_TYPE_DATETIME_TIMEZONE_GEOCLUE,
                                   IndicatorDatetimeTimezoneGeocluePriv);
  self->priv = p;

  geo_start (self);
}

/***
****  Public
***/

IndicatorDatetimeTimezone *
indicator_datetime_timezone_geoclue_new (void)
{
  gpointer o = g_object_new (INDICATOR_TYPE_DATETIME_TIMEZONE_GEOCLUE, NULL);

  return INDICATOR_DATETIME_TIMEZONE (o);
}
