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

#ifndef __INDICATOR_DATETIME_SERVICE_H__
#define __INDICATOR_DATETIME_SERVICE_H__

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

/* standard GObject macros */
#define INDICATOR_DATETIME_SERVICE(o)            (G_TYPE_CHECK_INSTANCE_CAST ((o), INDICATOR_TYPE_DATETIME_SERVICE, IndicatorDatetimeService))
#define INDICATOR_TYPE_DATETIME_SERVICE          (indicator_datetime_service_get_type())
#define INDICATOR_IS_DATETIME_SERVICE(o)         (G_TYPE_CHECK_INSTANCE_TYPE ((o), INDICATOR_TYPE_DATETIME_SERVICE))

typedef struct _IndicatorDatetimeService         IndicatorDatetimeService;
typedef struct _IndicatorDatetimeServiceClass    IndicatorDatetimeServiceClass;
typedef struct _IndicatorDatetimeServicePrivate  IndicatorDatetimeServicePrivate;

/* signal keys */
#define INDICATOR_DATETIME_SERVICE_SIGNAL_NAME_LOST   "name-lost"

/**
 * The Indicator Datetime Service.
 */
struct _IndicatorDatetimeService
{
  /*< private >*/
  GObject parent;
  IndicatorDatetimeServicePrivate * priv;
};

struct _IndicatorDatetimeServiceClass
{
  GObjectClass parent_class;

  /* signals */

  void (* name_lost)(IndicatorDatetimeService * self);
};

/***
****
***/

GType indicator_datetime_service_get_type (void);

IndicatorDatetimeService * indicator_datetime_service_new (gboolean replace);

GDateTime * indicator_datetime_service_get_localtime (IndicatorDatetimeService * service);

G_END_DECLS

#endif /* __INDICATOR_DATETIME_SERVICE_H__ */
