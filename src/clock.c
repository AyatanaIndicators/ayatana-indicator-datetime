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

#include "clock.h"

enum
{
  SIGNAL_CHANGED,
  SIGNAL_LAST
};

static guint signals[SIGNAL_LAST] = { 0 };

G_DEFINE_INTERFACE (IndicatorDatetimeClock,
                    indicator_datetime_clock,
                    0);

static void
indicator_datetime_clock_default_init (IndicatorDatetimeClockInterface * klass)
{
  signals[SIGNAL_CHANGED] = g_signal_new (
      "changed",
      G_TYPE_FROM_CLASS(klass),
      G_SIGNAL_RUN_LAST,
      G_STRUCT_OFFSET (IndicatorDatetimeClockInterface, changed),
      NULL, NULL,
      g_cclosure_marshal_VOID__VOID,
      G_TYPE_NONE, 0);
}

/***
****  PUBLIC API
***/

/**
 * Get a strv of timezones.
 *
 * Return value: (element-type char*)
 *               (transfer full):
 *               array of timezone strings
 */
gchar **
indicator_datetime_clock_get_timezones (IndicatorDatetimeClock * self)
{
  gchar ** timezones;
  IndicatorDatetimeClockInterface * iface;

  g_return_val_if_fail (INDICATOR_IS_DATETIME_CLOCK(self), NULL);
  iface = INDICATOR_DATETIME_CLOCK_GET_INTERFACE(self);

  if (iface->get_timezones != NULL)
    timezones = iface->get_timezones (self);
  else
    timezones = NULL;

  return timezones;
}

/**
 * Get the current time.
 *
 * Return value: (element-type GDateTime*)
 *               (transfer full):
 *               the current time.
 */
GDateTime *
indicator_datetime_clock_get_localtime (IndicatorDatetimeClock * self)
{
  GDateTime * now;
  IndicatorDatetimeClockInterface * iface;

  g_return_val_if_fail (INDICATOR_IS_DATETIME_CLOCK(self), NULL);
  iface = INDICATOR_DATETIME_CLOCK_GET_INTERFACE(self);

  if (iface->get_localtime != NULL)
    now = iface->get_localtime (self);
  else
    now = NULL;

  return now;
}

/**
 * Emits the "changed" signal.
 *
 * This should only be called by subclasses.
 */
void
indicator_datetime_clock_emit_changed (IndicatorDatetimeClock * self)
{
  g_return_if_fail (INDICATOR_IS_DATETIME_CLOCK (self));

  g_signal_emit (self, signals[SIGNAL_CHANGED], 0, NULL);
}
