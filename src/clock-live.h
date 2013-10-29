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

#ifndef __INDICATOR_DATETIME_CLOCK_LIVE__H__
#define __INDICATOR_DATETIME_CLOCK_LIVE__H__

#include <glib-object.h> /* parent class */

#include "clock.h"

G_BEGIN_DECLS

#define INDICATOR_TYPE_DATETIME_CLOCK_LIVE \
  (indicator_datetime_clock_live_get_type())

#define INDICATOR_DATETIME_CLOCK_LIVE(o) \
  (G_TYPE_CHECK_INSTANCE_CAST ((o), \
                               INDICATOR_TYPE_DATETIME_CLOCK_LIVE, \
                               IndicatorDatetimeClockLive))

#define INDICATOR_DATETIME_CLOCK_LIVE_GET_CLASS(o) \
 (G_TYPE_INSTANCE_GET_CLASS ((o), \
                             INDICATOR_TYPE_DATETIME_CLOCK_LIVE, \
                             IndicatorDatetimeClockLiveClass))

#define INDICATOR_IS_DATETIME_CLOCK_LIVE(o) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((o), \
                               INDICATOR_TYPE_DATETIME_CLOCK_LIVE))

typedef struct _IndicatorDatetimeClockLive
                IndicatorDatetimeClockLive;
typedef struct _IndicatorDatetimeClockLivePriv
                IndicatorDatetimeClockLivePriv;
typedef struct _IndicatorDatetimeClockLiveClass
                IndicatorDatetimeClockLiveClass;

/**
 * An IndicatorDatetimeClock which gives live clock times
 * from timezones determined by geoclue and /etc/timezone
 */
struct _IndicatorDatetimeClockLive
{
  GObject parent_instance;

  IndicatorDatetimeClockLivePriv * priv;
};

struct _IndicatorDatetimeClockLiveClass
{
  GObjectClass parent_class;
};

IndicatorDatetimeClock * indicator_datetime_clock_live_new (void);

G_END_DECLS

#endif /* __INDICATOR_DATETIME_CLOCK_LIVE__H__ */
