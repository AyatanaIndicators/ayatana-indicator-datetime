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

#include <geoclue/geoclue-master.h>
#include <geoclue/geoclue-master-client.h>

#include "timezone-geoclue.h"

struct _IndicatorDatetimeTimezoneGeocluePriv
{
  GeoclueMaster * master;
  GeoclueMasterClient * client;
  GeoclueAddress * address;
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
on_address_changed (GeoclueAddress  * address     G_GNUC_UNUSED,
                    int               timestamp   G_GNUC_UNUSED,
                    GHashTable      * addy_data,
                    GeoclueAccuracy * accuracy    G_GNUC_UNUSED,
                    GError          * error,
                    gpointer          gself)
{
  if (error != NULL)
    {
      g_warning ("%s Unable to get timezone from GeoClue: %s", G_STRFUNC, error->message);
    }
  else
    {
      IndicatorDatetimeTimezoneGeoclue * self = INDICATOR_DATETIME_TIMEZONE_GEOCLUE (gself);
      const char * timezone = g_hash_table_lookup (addy_data, "timezone");
      indicator_datetime_timezone_set_timezone (INDICATOR_DATETIME_TIMEZONE(self), timezone);
    }
}

/* The signal doesn't have the parameter for an error, so it ends up needing
   a NULL inserted. */
static void
on_address_changed_sig (GeoclueAddress  * address     G_GNUC_UNUSED,
                        int               timestamp   G_GNUC_UNUSED,
                        GHashTable      * addy_data,
                        GeoclueAccuracy * accuracy    G_GNUC_UNUSED,
                        gpointer          gself)
{
  return on_address_changed(address, timestamp, addy_data, accuracy, NULL, gself);
}

static void
on_address_created (GeoclueMasterClient * master   G_GNUC_UNUSED,
                    GeoclueAddress      * address,
                    GError              * error,
                    gpointer              gself)
{
  if (error != NULL)
    {
      g_warning ("%s Unable to get timezone from GeoClue: %s", G_STRFUNC, error->message);
    }
  else
    {
      priv_t * p = INDICATOR_DATETIME_TIMEZONE_GEOCLUE(gself)->priv;

      g_assert (p->address == NULL);
      p->address = g_object_ref (address);

      geoclue_address_get_address_async (address, on_address_changed, gself);
      g_signal_connect (address, "address-changed", G_CALLBACK(on_address_changed_sig), gself);
    }
}

static void
on_requirements_set (GeoclueMasterClient * master     G_GNUC_UNUSED,
                     GError              * error,
                     gpointer              user_data  G_GNUC_UNUSED)
{
  if (error != NULL)
    {
      g_warning ("%s Unable to get timezone from GeoClue: %s", G_STRFUNC, error->message);
    }
}

static void
on_client_created (GeoclueMaster        * master   G_GNUC_UNUSED,
                   GeoclueMasterClient  * client,
                   gchar                * path,
                   GError               * error,
                   gpointer               gself)
{
  g_debug ("Created Geoclue client at: %s", path);

  if (error != NULL)
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
      g_signal_handlers_disconnect_by_func (p->address, on_address_changed_sig, self);
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

static void
my_dispose (GObject * o)
{
  geo_stop (INDICATOR_DATETIME_TIMEZONE_GEOCLUE (o));

  G_OBJECT_CLASS (indicator_datetime_timezone_geoclue_parent_class)->dispose (o);
}

static void
indicator_datetime_timezone_geoclue_class_init (IndicatorDatetimeTimezoneGeoclueClass * klass)
{
  GObjectClass * object_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->dispose = my_dispose;

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
