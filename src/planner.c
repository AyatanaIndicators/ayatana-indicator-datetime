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

#include "planner.h"

/**
***  Signals Boilerplate
**/

enum
{
  SIGNAL_APPTS_CHANGED,
  SIGNAL_LAST
};

static guint signals[SIGNAL_LAST] = { 0 };

/**
***  Properties Boilerplate
**/

enum
{
  PROP_0,
  PROP_TIMEZONE,
  PROP_LAST
};

static GParamSpec * properties[PROP_LAST] = { 0 };

/**
*** GObject Boilerplate
**/

G_DEFINE_TYPE (IndicatorDatetimePlanner,
               indicator_datetime_planner,
               G_TYPE_OBJECT)

struct _IndicatorDatetimePlannerPriv
{
  char * timezone;
};

/***
**** GObjectClass virtual funcs
***/

static void
my_get_property (GObject     * o,
                 guint         property_id,
                 GValue      * value,
                 GParamSpec  * pspec)
{
  IndicatorDatetimePlanner * self = INDICATOR_DATETIME_PLANNER (o);

  switch (property_id)
    {
      case PROP_TIMEZONE:
        g_value_set_string (value, self->priv->timezone);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (o, property_id, pspec);
    }
}

static void
my_set_property (GObject       * o,
                 guint           property_id,
                 const GValue  * value,
                 GParamSpec    * pspec)
{
  IndicatorDatetimePlanner * self = INDICATOR_DATETIME_PLANNER (o);

  switch (property_id)
    {
      case PROP_TIMEZONE:
        indicator_datetime_planner_set_timezone (self, g_value_get_string (value));
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (o, property_id, pspec);
    }
}

static void
my_finalize (GObject * o)
{
  IndicatorDatetimePlanner * self = INDICATOR_DATETIME_PLANNER(o);

  g_free (self->priv->timezone);

  G_OBJECT_CLASS (indicator_datetime_planner_parent_class)->finalize (o);
}

/***
****  Instantiation
***/

static void
indicator_datetime_planner_class_init (IndicatorDatetimePlannerClass * klass)
{
  GObjectClass * object_class;
  const GParamFlags flags = G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS;

  g_type_class_add_private (klass, sizeof (IndicatorDatetimePlannerPriv));

  object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = my_finalize;
  object_class->get_property = my_get_property;
  object_class->set_property = my_set_property;

  klass->get_appointments = NULL;

  signals[SIGNAL_APPTS_CHANGED] = g_signal_new ("appointments-changed",
                                                G_TYPE_FROM_CLASS(klass),
                                                G_SIGNAL_RUN_LAST,
                                                G_STRUCT_OFFSET (IndicatorDatetimePlannerClass, appointments_changed),
                                                NULL, NULL,
                                                g_cclosure_marshal_VOID__VOID,
                                                G_TYPE_NONE, 0);

  /* install properties */

  properties[PROP_0] = NULL;

  properties[PROP_TIMEZONE] = g_param_spec_string ("timezone",
                                                   "Timezone",
                                                   "Default timezone for the EDS appointments",
                                                   "",
                                                   flags);

  g_object_class_install_properties (object_class, PROP_LAST, properties);
}

static void
indicator_datetime_planner_init (IndicatorDatetimePlanner * self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            INDICATOR_TYPE_DATETIME_PLANNER,
                                            IndicatorDatetimePlannerPriv);
}

/***
****  Public API
***/

void
indicator_datetime_planner_emit_appointments_changed (IndicatorDatetimePlanner * self)
{
  g_return_if_fail (INDICATOR_IS_DATETIME_PLANNER (self));

  g_signal_emit (self, signals[SIGNAL_APPTS_CHANGED], 0, NULL);
}

static gint
compare_appointments_by_start_time (gconstpointer ga, gconstpointer gb)
{
  const struct IndicatorDatetimeAppt * a = ga;
  const struct IndicatorDatetimeAppt * b = gb;

  return g_date_time_compare (a->begin, b->begin);
}

GSList *
indicator_datetime_planner_get_appointments (IndicatorDatetimePlanner * self, GDateTime * begin, GDateTime * end)
{
  GSList * appointments;

  g_return_val_if_fail (INDICATOR_IS_DATETIME_PLANNER (self), NULL);

  appointments = INDICATOR_DATETIME_PLANNER_GET_CLASS (self)->get_appointments (self, begin, end);
  return g_slist_sort (appointments, compare_appointments_by_start_time);
}

gboolean
indicator_datetime_planner_is_configured (IndicatorDatetimePlanner * self)
{
  g_return_val_if_fail (INDICATOR_IS_DATETIME_PLANNER (self), FALSE);

  return INDICATOR_DATETIME_PLANNER_GET_CLASS (self)->is_configured (self);
}

void
indicator_datetime_planner_activate (IndicatorDatetimePlanner * self)
{
  g_return_if_fail (INDICATOR_IS_DATETIME_PLANNER (self));

  INDICATOR_DATETIME_PLANNER_GET_CLASS (self)->activate (self);
}

void
indicator_datetime_planner_activate_time (IndicatorDatetimePlanner * self, GDateTime * time)
{
  g_return_if_fail (INDICATOR_IS_DATETIME_PLANNER (self));

  INDICATOR_DATETIME_PLANNER_GET_CLASS (self)->activate_time (self, time);
}

void
indicator_datetime_planner_set_timezone (IndicatorDatetimePlanner * self, const char * timezone)
{
  g_return_if_fail (INDICATOR_IS_DATETIME_PLANNER (self));

  g_free (self->priv->timezone);
  self->priv->timezone = g_strdup (timezone);
  g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_TIMEZONE]);
}

const char *
indicator_datetime_planner_get_timezone (IndicatorDatetimePlanner * self)
{
  g_return_val_if_fail (INDICATOR_IS_DATETIME_PLANNER (self), NULL);

  return self->priv->timezone;
}
