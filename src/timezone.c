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

#include "timezone.h"

G_DEFINE_TYPE (IndicatorDatetimeTimezone,
               indicator_datetime_timezone,
               G_TYPE_OBJECT)

enum
{
  PROP_0,
  PROP_TIMEZONE,
  PROP_LAST
};

static GParamSpec * properties[PROP_LAST] = { 0, };

static void
my_get_property (GObject     * o,
                 guint         property_id,
                 GValue      * value,
                 GParamSpec  * pspec)
{
  IndicatorDatetimeTimezone * self = INDICATOR_DATETIME_TIMEZONE (o);

  switch (property_id)
    {
      case PROP_TIMEZONE:
        g_value_set_string (value, indicator_datetime_timezone_get_timezone (self));
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (o, property_id, pspec);
    }
}

static void
my_dispose (GObject * object)
{
  G_OBJECT_CLASS (indicator_datetime_timezone_parent_class)->dispose (object);
}

static void
/* cppcheck-suppress unusedFunction */
indicator_datetime_timezone_class_init (IndicatorDatetimeTimezoneClass * klass)
{
  GObjectClass * object_class;
  const GParamFlags flags = G_PARAM_READABLE | G_PARAM_STATIC_STRINGS;

  object_class = G_OBJECT_CLASS (klass);
  object_class->get_property = my_get_property;
  object_class->dispose = my_dispose;

  klass->get_timezone = NULL;

  properties[PROP_TIMEZONE] = g_param_spec_string ("timezone",
                                                   "Timezone",
                                                   "Timezone",
                                                   "",
                                                   flags);

  g_object_class_install_properties (object_class, PROP_LAST, properties);
}

static void
indicator_datetime_timezone_init (IndicatorDatetimeTimezone * self G_GNUC_UNUSED)
{
}

/***
****
***/

const char *
indicator_datetime_timezone_get_timezone (IndicatorDatetimeTimezone * self)
{
  g_return_val_if_fail (INDICATOR_IS_DATETIME_TIMEZONE (self), NULL);

  return INDICATOR_DATETIME_TIMEZONE_GET_CLASS (self)->get_timezone (self);
}

void
indicator_datetime_timezone_notify_timezone (IndicatorDatetimeTimezone * self)
{
  g_return_if_fail (INDICATOR_IS_DATETIME_TIMEZONE (self));

  g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_TIMEZONE]);
}

