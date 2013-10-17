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

#ifndef __INDICATOR_DATETIME_CLOCK__H__
#define __INDICATOR_DATETIME_CLOCK__H__

#include <glib-object.h>

G_BEGIN_DECLS

#define INDICATOR_TYPE_DATETIME_CLOCK \
  (indicator_datetime_clock_get_type ())

#define INDICATOR_DATETIME_CLOCK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                               INDICATOR_TYPE_DATETIME_CLOCK, \
                               IndicatorDatetimeClock))

#define INDICATOR_IS_DATETIME_CLOCK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), INDICATOR_TYPE_DATETIME_CLOCK))

#define INDICATOR_DATETIME_CLOCK_GET_INTERFACE(inst) \
  (G_TYPE_INSTANCE_GET_INTERFACE ((inst), \
                                  INDICATOR_TYPE_DATETIME_CLOCK, \
                                  IndicatorDatetimeClockInterface))

typedef struct _IndicatorDatetimeClock
                IndicatorDatetimeClock;

typedef struct _IndicatorDatetimeClockInterface
                IndicatorDatetimeClockInterface;

struct _IndicatorDatetimeClockInterface
{
  GTypeInterface parent_iface;

  /* signals */
  void (*changed) (IndicatorDatetimeClock * self);

  /* virtual functions */
  gchar** (*get_timezones) (IndicatorDatetimeClock * self);
  GDateTime* (*get_localtime) (IndicatorDatetimeClock * self);
};

GType indicator_datetime_clock_get_type (void);

/***
****
***/

gchar    ** indicator_datetime_clock_get_timezones    (IndicatorDatetimeClock * clock);

GDateTime * indicator_datetime_clock_get_localtime    (IndicatorDatetimeClock * clock);

void        indicator_datetime_clock_emit_changed     (IndicatorDatetimeClock * clock);


G_END_DECLS

#endif /* __INDICATOR_DATETIME_CLOCK__H__ */
