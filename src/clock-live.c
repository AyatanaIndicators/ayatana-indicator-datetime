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

#include <glib.h>
#include <gio/gio.h>

#include "config.h"

#include "clock-live.h"
#include "settings-shared.h"
#include "timezone-file.h"
#include "timezone-geoclue.h"

/***
****  private struct
***/

struct _IndicatorDatetimeClockLivePriv
{
  GSettings * settings;

  IndicatorDatetimeTimezone * tz_file;
  IndicatorDatetimeTimezone * tz_geoclue;
  gchar ** timezones;
  GTimeZone * localtime_zone;
};

typedef IndicatorDatetimeClockLivePriv priv_t;

/***
****  GObject boilerplate
***/

static void indicator_datetime_clock_interface_init (
                                IndicatorDatetimeClockInterface * iface);

G_DEFINE_TYPE_WITH_CODE (
  IndicatorDatetimeClockLive,
  indicator_datetime_clock_live,
  G_TYPE_OBJECT,
  G_IMPLEMENT_INTERFACE (INDICATOR_TYPE_DATETIME_CLOCK,
                         indicator_datetime_clock_interface_init));

/***
****  Timezones
***/

static void
on_current_timezone_changed (IndicatorDatetimeClockLive * self)
{
  priv_t * p = self->priv;

  g_clear_pointer (&p->timezones, g_strfreev);
  g_clear_pointer (&p->localtime_zone, g_time_zone_unref);

  indicator_datetime_clock_emit_changed (INDICATOR_DATETIME_CLOCK (self));
}

static void
set_detect_location_enabled (IndicatorDatetimeClockLive * self, gboolean enabled)
{
  priv_t * p = self->priv;
  gboolean changed = FALSE;

  /* geoclue */

  if (!p->tz_geoclue && enabled)
    {
      p->tz_geoclue = indicator_datetime_timezone_geoclue_new ();
      g_signal_connect_swapped (p->tz_geoclue, "notify::timezone",
                                G_CALLBACK(on_current_timezone_changed),
                                self);
      changed = TRUE;
    }
  else if (p->tz_geoclue && !enabled)
    {
      g_signal_handlers_disconnect_by_func (p->tz_geoclue,
                                            on_current_timezone_changed,
                                            self);
      g_clear_object (&p->tz_geoclue);
      changed = TRUE;
    }

  /* timezone file */

  if (!p->tz_file && enabled)
    {
      p->tz_file = indicator_datetime_timezone_file_new (TIMEZONE_FILE);
      g_signal_connect_swapped (p->tz_file, "notify::timezone",
                                G_CALLBACK(on_current_timezone_changed),
                                self);
      changed = TRUE;
    }
  else if (p->tz_file && !enabled)
    {
      g_signal_handlers_disconnect_by_func (p->tz_file,
                                            on_current_timezone_changed,
                                            self);
      g_clear_object (&p->tz_file);
      changed = TRUE;
    }

  if (changed)
    on_current_timezone_changed (self);
}

/* When the 'auto-detect timezone' boolean setting changes,
   start or stop watching geoclue and /etc/timezone */
static void
on_detect_location_changed (IndicatorDatetimeClockLive * self)
{
  const gboolean enabled = g_settings_get_boolean (self->priv->settings, SETTINGS_SHOW_DETECTED_S);
  set_detect_location_enabled (self, enabled);
}

/***
****  IndicatorDatetimeClock virtual functions
***/

static void
rebuild_timezones (IndicatorDatetimeClockLive * self)
{
  priv_t * p;
  GHashTable * hash;
  int i;
  GHashTableIter iter;
  gpointer key;

  p = self->priv;

  hash = g_hash_table_new (g_str_hash, g_str_equal);

  if (p->tz_file != NULL)
    {
      const gchar * tz = indicator_datetime_timezone_get_timezone (p->tz_file);
      if (tz && *tz)
        g_hash_table_add (hash, (gpointer) tz);
    }

  if (p->tz_geoclue != NULL)
    {
      const gchar * tz = indicator_datetime_timezone_get_timezone (p->tz_geoclue);
      if (tz && *tz)
        g_hash_table_add (hash, (gpointer) tz);
    }

  g_strfreev (p->timezones);
  p->timezones = g_new0 (gchar*, g_hash_table_size(hash) + 1);
  i = 0;
  g_hash_table_iter_init (&iter, hash);
  while (g_hash_table_iter_next (&iter, &key, NULL))
    p->timezones[i++] = g_strdup (key);
  g_hash_table_unref (hash);

  g_clear_pointer (&p->localtime_zone, g_time_zone_unref);
  p->localtime_zone = g_time_zone_new (p->timezones ? p->timezones[0] : NULL);
}

static const gchar **
my_get_timezones (IndicatorDatetimeClock * clock)
{
  IndicatorDatetimeClockLive * self = INDICATOR_DATETIME_CLOCK_LIVE (clock);
  priv_t * p = self->priv;

  if (G_UNLIKELY (p->timezones == NULL))
    rebuild_timezones (self);

  return (const gchar **) p->timezones;
}

static GDateTime *
my_get_localtime (IndicatorDatetimeClock * clock)
{
  IndicatorDatetimeClockLive * self = INDICATOR_DATETIME_CLOCK_LIVE (clock);
  priv_t * p = self->priv;

  if (G_UNLIKELY (p->localtime_zone == NULL))
    rebuild_timezones (self);

  return g_date_time_new_now (p->localtime_zone);
}

/***
****  GObject virtual functions
***/

static void
my_dispose (GObject * o)
{
  IndicatorDatetimeClockLive * self;
  priv_t * p;

  self = INDICATOR_DATETIME_CLOCK_LIVE(o);
  p = self->priv;

  if (p->settings != NULL)
    {
      g_signal_handlers_disconnect_by_data (p->settings, self);
      g_clear_object (&p->settings);
    }

  set_detect_location_enabled (self, FALSE);

  G_OBJECT_CLASS (indicator_datetime_clock_live_parent_class)->dispose (o);
}

static void
my_finalize (GObject * o)
{
  IndicatorDatetimeClockLive * self;
  priv_t * p;

  self = INDICATOR_DATETIME_CLOCK_LIVE(o);
  p = self->priv;

  g_clear_pointer (&p->localtime_zone, g_time_zone_unref);
  g_strfreev (p->timezones);

  G_OBJECT_CLASS (indicator_datetime_clock_live_parent_class)->dispose (o);
}

/***
****  Instantiation
***/

static void
indicator_datetime_clock_live_class_init (IndicatorDatetimeClockLiveClass * klass)
{
  GObjectClass * object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = my_dispose;
  object_class->finalize = my_finalize;

  g_type_class_add_private (klass,
                            sizeof (IndicatorDatetimeClockLivePriv));
}

static void
indicator_datetime_clock_interface_init (IndicatorDatetimeClockInterface * iface)
{
  iface->get_localtime = my_get_localtime;
  iface->get_timezones = my_get_timezones;
}

static void
indicator_datetime_clock_live_init (IndicatorDatetimeClockLive * self)
{
  IndicatorDatetimeClockLivePriv * p;

  p = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                   INDICATOR_TYPE_DATETIME_CLOCK_LIVE,
                                   IndicatorDatetimeClockLivePriv);
  self->priv = p;

  p->settings = g_settings_new (SETTINGS_INTERFACE);
  g_signal_connect (p->settings, "changed::" SETTINGS_SHOW_DETECTED_S,
                    G_CALLBACK(on_detect_location_changed), self);


  on_detect_location_changed (self);
}

/***
****  Public API
***/

IndicatorDatetimeClock *
indicator_datetime_clock_live_new (void)
{
  gpointer o = g_object_new (INDICATOR_TYPE_DATETIME_CLOCK_LIVE, NULL);

  return INDICATOR_DATETIME_CLOCK (o);
}
