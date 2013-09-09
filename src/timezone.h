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

#ifndef __INDICATOR_DATETIME_TIMEZONE__H__
#define __INDICATOR_DATETIME_TIMEZONE__H__

#include <glib.h>
#include <glib-object.h> /* parent class */

G_BEGIN_DECLS

#define INDICATOR_TYPE_DATETIME_TIMEZONE          (indicator_datetime_timezone_get_type())
#define INDICATOR_DATETIME_TIMEZONE(o)            (G_TYPE_CHECK_INSTANCE_CAST ((o), INDICATOR_TYPE_DATETIME_TIMEZONE, IndicatorDatetimeTimezone))
#define INDICATOR_DATETIME_TIMEZONE_GET_CLASS(o)  (G_TYPE_INSTANCE_GET_CLASS ((o), INDICATOR_TYPE_DATETIME_TIMEZONE, IndicatorDatetimeTimezoneClass))
#define INDICATOR_DATETIME_TIMEZONE_CLASS(k)      (G_TYPE_CHECK_CLASS_CAST ((k), INDICATOR_TYPE_DATETIME_TIMEZONE, IndicatorDatetimeTimezoneClass))
#define INDICATOR_IS_DATETIME_TIMEZONE(o)         (G_TYPE_CHECK_INSTANCE_TYPE ((o), INDICATOR_TYPE_DATETIME_TIMEZONE))

typedef struct _IndicatorDatetimeTimezone        IndicatorDatetimeTimezone;
typedef struct _IndicatorDatetimeTimezonePriv    IndicatorDatetimeTimezonePriv;
typedef struct _IndicatorDatetimeTimezoneClass   IndicatorDatetimeTimezoneClass;

GType indicator_datetime_timezone_get_type (void);

#define INDICATOR_DATETIME_TIMEZONE_PROPERTY_TIMEZONE "timezone"

/**
 * Abstract Base Class for objects that provide a timezone.
 *
 * We use this in datetime to determine the user's current timezone
 * for display in the 'locations' section of the datetime indicator.
 *
 * This class has a 'timezone' property that clients can watch
 * for change notifications.
 */
struct _IndicatorDatetimeTimezone
{
  /*< private >*/
  GObject parent;
  IndicatorDatetimeTimezonePriv * priv;
};

struct _IndicatorDatetimeTimezoneClass
{
  GObjectClass parent_class;
};

/***
****
***/

const char * indicator_datetime_timezone_get_timezone    (IndicatorDatetimeTimezone *);

void         indicator_datetime_timezone_set_timezone    (IndicatorDatetimeTimezone *,
                                                          const char * new_timezone);

G_END_DECLS

#endif /* __INDICATOR_DATETIME_TIMEZONE__H__ */
