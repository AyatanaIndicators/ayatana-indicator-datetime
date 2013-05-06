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

#ifndef __INDICATOR_DATETIME_TIMEZONE_FILE__H__
#define __INDICATOR_DATETIME_TIMEZONE_FILE__H__

#include <glib.h>
#include <glib-object.h>

#include "timezone.h" /* parent class */

G_BEGIN_DECLS

#define INDICATOR_TYPE_DATETIME_TIMEZONE_FILE          (indicator_datetime_timezone_file_get_type())
#define INDICATOR_DATETIME_TIMEZONE_FILE(o)            (G_TYPE_CHECK_INSTANCE_CAST ((o), INDICATOR_TYPE_DATETIME_TIMEZONE_FILE, IndicatorDatetimeTimezoneFile))
#define INDICATOR_DATETIME_TIMEZONE_FILE_GET_CLASS(o)  (G_TYPE_INSTANCE_GET_CLASS ((o), INDICATOR_TYPE_DATETIME_TIMEZONE_FILE, IndicatorDatetimeTimezoneFileClass))
#define INDICATOR_IS_DATETIME_TIMEZONE_FILE(o)         (G_TYPE_CHECK_INSTANCE_TYPE ((o), INDICATOR_TYPE_DATETIME_TIMEZONE_FILE))

typedef struct _IndicatorDatetimeTimezoneFile        IndicatorDatetimeTimezoneFile;
typedef struct _IndicatorDatetimeTimezoneFilePriv    IndicatorDatetimeTimezoneFilePriv;
typedef struct _IndicatorDatetimeTimezoneFileClass   IndicatorDatetimeTimezoneFileClass;

GType indicator_datetime_timezone_file_get_type (void);

/**
 * An implementation of IndicatorDatetimeTimezone which determines the timezone
 * from monitoring a local file, such as /etc/timezone
 */
struct _IndicatorDatetimeTimezoneFile
{
  /*< private >*/
  IndicatorDatetimeTimezone parent;
  IndicatorDatetimeTimezoneFilePriv * priv;
};

struct _IndicatorDatetimeTimezoneFileClass
{
  IndicatorDatetimeTimezoneClass parent_class;
};

IndicatorDatetimeTimezone * indicator_datetime_timezone_file_new (const char * filename);

G_END_DECLS

#endif /* __INDICATOR_DATETIME_TIMEZONE_FILE__H__ */
