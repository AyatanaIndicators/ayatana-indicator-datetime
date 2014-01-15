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

#include "config.h"

#include "planner-mock.h"

struct _IndicatorDatetimePlannerMockPriv
{
  gboolean is_configured;
};

typedef IndicatorDatetimePlannerMockPriv priv_t;

G_DEFINE_TYPE (IndicatorDatetimePlannerMock,
               indicator_datetime_planner_mock,
               INDICATOR_TYPE_DATETIME_PLANNER)

/***
****  IndicatorDatetimePlanner virtual funcs
***/

static void
my_get_appointments (IndicatorDatetimePlanner  * planner,
                     GDateTime                 * begin_datetime,
                     GDateTime                 * /*end_datetime*/,
                     GAsyncReadyCallback         callback,
                     gpointer                    user_data)
{
  GTask * task;
  GSList * appointments;
  struct IndicatorDatetimeAppt * appt;
  struct IndicatorDatetimeAppt * prev;

  task = g_task_new (planner, NULL, callback, user_data);

  /**
  ***  Build the appointments list
  **/

  appointments = NULL;

  /* add a daily appointment that occurs at the beginning of the next minute */
  appt = g_slice_new0 (struct IndicatorDatetimeAppt);
  appt->is_daily = TRUE;
  appt->begin = g_date_time_add_seconds (begin_datetime, 60-g_date_time_get_seconds(begin_datetime));
  appt->end = g_date_time_add_minutes (appt->begin, 1);
  appt->color = g_strdup ("#00FF00");
  appt->is_event = TRUE;
  appt->summary = g_strdup ("Daily alarm");
  appt->uid = g_strdup ("this uid isn't very random.");
  appt->has_alarms = TRUE;
  appt->url = g_strdup ("alarm:///some-alarm-info-goes-here");
  appointments = g_slist_prepend (appointments, appt);
  prev = appt;

  /* and add one for a minute later that has an alarm uri */
  appt = g_slice_new0 (struct IndicatorDatetimeAppt);
  appt->is_daily = TRUE;
  appt->begin = g_date_time_add_minutes (prev->end, 1);
  appt->end = g_date_time_add_minutes (appt->begin, 1);
  appt->color = g_strdup ("#0000FF");
  appt->is_event = TRUE;
  appt->summary = g_strdup ("Second Daily alarm");
  appt->uid = g_strdup ("this uid isn't very random either.");
  appt->has_alarms = FALSE;
  appointments = g_slist_prepend (appointments, appt);

  /* done */
  g_task_return_pointer (task, appointments, NULL);
  g_object_unref (task);
}

static GSList *
my_get_appointments_finish (IndicatorDatetimePlanner* /*self*/,
                            GAsyncResult*               res,
                            GError**                    error)
{
  return g_task_propagate_pointer(G_TASK(res), error);
}

static gboolean
my_is_configured(IndicatorDatetimePlanner* planner)
{
  IndicatorDatetimePlannerMock * self;
  self = INDICATOR_DATETIME_PLANNER_MOCK(planner);
  return self->priv->is_configured;
}

static void
my_activate(IndicatorDatetimePlanner* /*self*/)
{
  g_message("%s %s", G_STRLOC, G_STRFUNC);
}

static void
my_activate_time(IndicatorDatetimePlanner* /*self*/,
                 GDateTime*                  activate_time)
{
  gchar * str = g_date_time_format(activate_time, "%F %T");
  g_message("%s %s: %s", G_STRLOC, G_STRFUNC, str);
  g_free(str);
}

/***
****  GObject virtual funcs
***/

static void
my_dispose(GObject * o)
{
  G_OBJECT_CLASS(indicator_datetime_planner_mock_parent_class)->dispose(o);
}

/***
****  Instantiation
***/

static void
indicator_datetime_planner_mock_class_init(IndicatorDatetimePlannerMockClass* klass)
{
  GObjectClass * object_class;
  IndicatorDatetimePlannerClass * planner_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->dispose = my_dispose;

  planner_class = INDICATOR_DATETIME_PLANNER_CLASS (klass);
  planner_class->is_configured = my_is_configured;
  planner_class->activate = my_activate;
  planner_class->activate_time = my_activate_time;
  planner_class->get_appointments = my_get_appointments;
  planner_class->get_appointments_finish = my_get_appointments_finish;

  g_type_class_add_private (klass, sizeof (IndicatorDatetimePlannerMockPriv));
}

static void
indicator_datetime_planner_mock_init (IndicatorDatetimePlannerMock * self)
{
  priv_t * p;

  p = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                   INDICATOR_TYPE_DATETIME_PLANNER_MOCK,
                                   IndicatorDatetimePlannerMockPriv);

  p->is_configured = TRUE;

  self->priv = p;
}

/***
****  Public
***/

IndicatorDatetimePlanner *
indicator_datetime_planner_mock_new (void)
{
  gpointer o = g_object_new (INDICATOR_TYPE_DATETIME_PLANNER_MOCK, NULL);

  return INDICATOR_DATETIME_PLANNER (o);
}
