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

#ifndef __INDICATOR_DATETIME_LOCATION__H__
#define __INDICATOR_DATETIME_LOCATION__H__

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define INDICATOR_TYPE_DATETIME_LOCATION          (indicator_datetime_location_get_type())
#define INDICATOR_DATETIME_LOCATION(o)            (G_TYPE_CHECK_INSTANCE_CAST ((o), INDICATOR_TYPE_DATETIME_LOCATION, IndicatorDatetimeLocation))
#define INDICATOR_DATETIME_LOCATION_GET_CLASS(o)  (G_TYPE_INSTANCE_GET_CLASS ((o), INDICATOR_TYPE_DATETIME_LOCATION, IndicatorDatetimeLocationClass))
#define INDICATOR_DATETIME_LOCATION_CLASS(k)      (G_TYPE_CHECK_CLASS_CAST ((k), INDICATOR_TYPE_DATETIME_LOCATION, IndicatorDatetimeLocationClass))
#define INDICATOR_IS_DATETIME_LOCATION(o)         (G_TYPE_CHECK_INSTANCE_TYPE ((o), INDICATOR_TYPE_DATETIME_LOCATION))

typedef struct _IndicatorDatetimeLocation        IndicatorDatetimeLocation;
typedef struct _IndicatorDatetimeLocationClass   IndicatorDatetimeLocationClass;

GType indicator_datetime_location_get_type (void);

/**
 * Abstract Base Class for the mechanisms that determine timezone by location
 */
struct _IndicatorDatetimeLocation
{
  /*< private >*/
  GObject parent;
};

struct _IndicatorDatetimeLocationClass
{
  GObjectClass parent_class;

  /* virtual functions */
  const char * (*get_timezone) (IndicatorDatetimeLocation * self);
};

/***
****
***/

#define INDICATOR_DATETIME_LOCATION_PROPERTY_TIMEZONE "timezone"

const char * indicator_datetime_location_get_timezone    (IndicatorDatetimeLocation *);

void         indicator_datetime_location_notify_timezone (IndicatorDatetimeLocation *);

G_END_DECLS

#endif /* __INDICATOR_DATETIME_LOCATION__H__ */
