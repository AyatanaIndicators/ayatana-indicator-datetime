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

#include <libical/ical.h>
#include <libical/icaltime.h>
#include <libecal/libecal.h>
#include <libedataserver/libedataserver.h>

#include "planner-eds.h"

struct _IndicatorDatetimePlannerEdsPriv
{
  GSList * sources;
  GCancellable * cancellable;
  ESourceRegistry * source_registry;
};

typedef IndicatorDatetimePlannerEdsPriv priv_t;

G_DEFINE_TYPE (IndicatorDatetimePlannerEds,
               indicator_datetime_planner_eds,
               INDICATOR_TYPE_DATETIME_PLANNER)

G_DEFINE_QUARK ("source-client", source_client)

/***
****
***/

void
indicator_datetime_appt_free (struct IndicatorDatetimeAppt * appt)
{
  if (appt != NULL)
    {
      g_date_time_unref (appt->end);
      g_date_time_unref (appt->begin);
      g_free (appt->color);
      g_free (appt->summary);
      g_slice_free (struct IndicatorDatetimeAppt, appt);
    }
}

/***
**** my_get_appointments() helpers
***/

struct get_appointments_task_data
{
  /* how many subtasks are still running on */
  int subtask_count;

  /* the list of appointments to be returned */
  GSList * appointments;

  /* ensure that recurring events don't get multiple IndicatorDatetimeAppts */
  GHashTable * added;
};

static void
get_appointments_task_data_free (gpointer gdata)
{
  struct get_appointments_task_data * data = gdata;
  g_hash_table_unref (data->added);
  g_slice_free (struct get_appointments_task_data, data);
}

static void
on_all_subtasks_done (GTask * task)
{
  struct get_appointments_task_data * data = g_task_get_task_data (task);
  g_task_return_pointer (task, data->appointments, NULL);
  g_object_unref (task);
}

struct get_appointments_subtask_data
{
  GTask * task;

  gchar * color;
};

static void
on_subtask_done (gpointer gsubdata)
{
  struct get_appointments_subtask_data * subdata;
  GTask * task;
  struct get_appointments_task_data * data;

  subdata = gsubdata;
  task = subdata->task;

  /* free the subtask data */
  g_free (subdata->color);
  g_slice_free (struct get_appointments_subtask_data, subdata);

  /* poke the task */
  data = g_task_get_task_data (task);
  if (--data->subtask_count <= 0)
    on_all_subtasks_done (task);
}

static gboolean
my_get_appointments_foreach (ECalComponent * component,
                             time_t          begin,
                             time_t          end,
                             gpointer        gsubdata)
{
  const ECalComponentVType vtype = e_cal_component_get_vtype (component);
  struct get_appointments_subtask_data * subdata = gsubdata;
  struct get_appointments_task_data * data = g_task_get_task_data (subdata->task);

  if ((vtype == E_CAL_COMPONENT_EVENT) || (vtype == E_CAL_COMPONENT_TODO))
    {
      const gchar * uid = NULL;
      icalproperty_status status = 0;

      e_cal_component_get_uid (component, &uid);
      e_cal_component_get_status (component, &status);

      if ((uid != NULL) &&
          (!g_hash_table_contains (data->added, uid)) &&
          (status != ICAL_STATUS_COMPLETED) &&
          (status != ICAL_STATUS_CANCELLED))
        {
          GList * alarm_uids;
          GSList * l;
          GSList * recur_list;
          ECalComponentText text;
          struct IndicatorDatetimeAppt * appt;

          appt = g_slice_new0 (struct IndicatorDatetimeAppt);

          /* Determine whether this is a recurring event.
             NB: icalrecurrencetype supports complex recurrence patterns;
             however, since design only allows daily recurrence,
             that's all we support here. */
          e_cal_component_get_rrule_list (component, &recur_list);
          for (l=recur_list; l!=NULL; l=l->next)
            {
              const struct icalrecurrencetype * recur = l->data;
              appt->is_daily |= ((recur->freq == ICAL_DAILY_RECURRENCE)
                                  && (recur->interval == 1));
            }
          e_cal_component_free_recur_list (recur_list);

          text.value = "";
          e_cal_component_get_summary (component, &text);
 
          appt->begin = g_date_time_new_from_unix_local (begin);
          appt->end = g_date_time_new_from_unix_local (end);
          appt->color = g_strdup (subdata->color);
          appt->is_event = vtype == E_CAL_COMPONENT_EVENT;
          appt->summary = g_strdup (text.value);

          alarm_uids = e_cal_component_get_alarm_uids (component);
          appt->has_alarms = alarm_uids != NULL;
          cal_obj_uid_list_free (alarm_uids);

          data->appointments = g_slist_prepend (data->appointments, appt);
          g_hash_table_add (data->added, g_strdup(uid));
        }
    }

  return G_SOURCE_CONTINUE;
}

/***
****  IndicatorDatetimePlanner virtual funcs
***/

static void
my_get_appointments (IndicatorDatetimePlanner  * planner,
                     GDateTime                 * begin_datetime,
                     GDateTime                 * end_datetime,
                     GAsyncReadyCallback         callback,
                     gpointer                    user_data)
{
  GSList * l;
  priv_t * p;
  const char * str;
  icaltimezone * default_timezone;
  struct get_appointments_task_data * data;
  const int64_t begin = g_date_time_to_unix (begin_datetime);
  const int64_t end = g_date_time_to_unix (end_datetime);
  GTask * task;
  gboolean subtasks_added;

  p = INDICATOR_DATETIME_PLANNER_EDS (planner)->priv;

  /**
  ***  init the default timezone
  **/

  default_timezone = NULL;

  if ((str = indicator_datetime_planner_get_timezone (planner)))
    {
      default_timezone = icaltimezone_get_builtin_timezone (str);

      if (default_timezone == NULL) /* maybe str is a tzid? */
        default_timezone = icaltimezone_get_builtin_timezone_from_tzid (str);
    }

  /**
  ***  walk through the sources to build the appointment list
  **/

  data = g_slice_new0 (struct get_appointments_task_data);
  data->added = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  task = g_task_new (planner, p->cancellable, callback, user_data);
  g_task_set_task_data (task, data, get_appointments_task_data_free);

  subtasks_added = FALSE;
  for (l=p->sources; l!=NULL; l=l->next)
    {
      ESource * source;
      ECalClient * client;
      struct get_appointments_subtask_data * subdata;

      source = l->data;
      client = g_object_get_qdata (l->data, source_client_quark());
      if (client == NULL)
        continue;

      if (default_timezone != NULL)
        e_cal_client_set_default_timezone (client, default_timezone);

      subdata = g_slice_new (struct get_appointments_subtask_data);
      subdata->task = task;
      subdata->color = e_source_selectable_dup_color (e_source_get_extension (source, E_SOURCE_EXTENSION_CALENDAR));

      data->subtask_count++;
      subtasks_added = TRUE;
      e_cal_client_generate_instances (client,
                                       begin,
                                       end,
                                       p->cancellable,
                                       my_get_appointments_foreach,
                                       subdata,
                                       on_subtask_done);
    }

  if (!subtasks_added)
    on_all_subtasks_done (task);
}

static GSList *
my_get_appointments_finish (IndicatorDatetimePlanner  * self  G_GNUC_UNUSED,
                            GAsyncResult              * res,
                            GError                   ** error)
{
  return g_task_propagate_pointer (G_TASK(res), error);
}

gboolean
my_is_configured (IndicatorDatetimePlanner * planner)
{
  IndicatorDatetimePlannerEds * self;

  /* confirm that it's installed... */
  gchar *evo = g_find_program_in_path ("evolution");
  if (evo == NULL)
    return FALSE;

  g_debug ("found calendar app: '%s'", evo);
  g_free (evo);

  /* see if there are any calendar sources */
  self = INDICATOR_DATETIME_PLANNER_EDS (planner);
  return self->priv->sources != NULL;
}

static void
my_activate (IndicatorDatetimePlanner * self G_GNUC_UNUSED)
{
  GError * error = NULL;
  const char * const command = "evolution -c calendar";

  if (!g_spawn_command_line_async (command, &error))
    {
      g_warning ("Unable to start %s: %s", command, error->message);
      g_error_free (error);
    }
}

static void
my_activate_time (IndicatorDatetimePlanner * self G_GNUC_UNUSED,
                  GDateTime * activate_time)
{
  gchar * isodate;
  gchar * command;
  GError * err;

  isodate = g_date_time_format (activate_time, "%Y%m%d");
  command = g_strdup_printf ("evolution \"calendar:///?startdate=%s\"", isodate);
  err = 0;
  if (!g_spawn_command_line_async (command, &err))
    {
      g_warning ("Unable to start %s: %s", command, err->message);
      g_error_free (err);
    }

  g_free (command);
  g_free (isodate);
}

/***
****  Source / Client Wrangling
***/

static void
on_client_connected (GObject      * unused G_GNUC_UNUSED,
                     GAsyncResult * res,
                     gpointer       gself)
{
  GError * error;
  EClient * client;

  error = NULL;
  client = e_cal_client_connect_finish (res, &error);
  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("indicator-datetime cannot connect to EDS source: %s", error->message);

      g_error_free (error);
    }
  else
    {
      /* we've got a new connected ECalClient, so store it & notify clients */

      g_object_set_qdata_full (G_OBJECT(e_client_get_source(client)),
                               source_client_quark(),
                               client,
                               g_object_unref);

      indicator_datetime_planner_emit_appointments_changed (gself);
    }
}

static void
on_source_enabled (ESourceRegistry * registry  G_GNUC_UNUSED,
                   ESource         * source,
                   gpointer          gself)
{
  IndicatorDatetimePlannerEds * self = INDICATOR_DATETIME_PLANNER_EDS (gself);
  priv_t * p = self->priv;

  e_cal_client_connect (source,
                        E_CAL_CLIENT_SOURCE_TYPE_EVENTS,
                        p->cancellable,
                        on_client_connected,
                        self);
}

static void
on_source_added (ESourceRegistry * registry,
                 ESource         * source,
                 gpointer          gself)
{
  IndicatorDatetimePlannerEds * self = INDICATOR_DATETIME_PLANNER_EDS (gself);
  priv_t * p = self->priv;

  p->sources = g_slist_prepend (p->sources, g_object_ref(source));

  if (e_source_get_enabled (source))
    on_source_enabled (registry, source, gself);
}

static void
on_source_disabled (ESourceRegistry * registry  G_GNUC_UNUSED,
                    ESource         * source,
                    gpointer          gself)
{
  ECalClient * client;

  /* If this source has a connected ECalClient, remove it & notify clients */
  if ((client = g_object_steal_qdata (G_OBJECT(source), source_client_quark())))
    {
      g_object_unref (client);
      indicator_datetime_planner_emit_appointments_changed (gself);
    }
}

static void
on_source_removed (ESourceRegistry * registry,
                   ESource         * source,
                   gpointer          gself)
{
  IndicatorDatetimePlannerEds * self = INDICATOR_DATETIME_PLANNER_EDS (gself);
  priv_t * p = self->priv;

  on_source_disabled (registry, source, gself);

  p->sources = g_slist_remove (p->sources, source);
  g_object_unref (source);
}

static void
on_source_changed (ESourceRegistry * registry  G_GNUC_UNUSED,
                   ESource         * source    G_GNUC_UNUSED,
                   gpointer          gself)
{
  indicator_datetime_planner_emit_appointments_changed (gself);
}

static void
on_source_registry_ready (GObject      * source_object  G_GNUC_UNUSED,
                          GAsyncResult * res,
                          gpointer       gself)
{
  GError * error;
  ESourceRegistry * r;

  error = NULL;
  r = e_source_registry_new_finish (res, &error);
  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("indicator-datetime cannot show EDS appointments: %s", error->message);

      g_error_free (error);
    }
  else
    {
      IndicatorDatetimePlannerEds * self;
      priv_t * p;
      GList * l;
      GList * sources;

      self = INDICATOR_DATETIME_PLANNER_EDS (gself);
      p = self->priv;

      g_signal_connect (r, "source-added",    G_CALLBACK(on_source_added), self);
      g_signal_connect (r, "source-removed",  G_CALLBACK(on_source_removed), self);
      g_signal_connect (r, "source-changed",  G_CALLBACK(on_source_changed), self);
      g_signal_connect (r, "source-disabled", G_CALLBACK(on_source_disabled), self);
      g_signal_connect (r, "source-enabled",  G_CALLBACK(on_source_enabled), self);

      p->source_registry = r;

      sources = e_source_registry_list_sources (r, E_SOURCE_EXTENSION_CALENDAR);
      for (l=sources; l!=NULL; l=l->next)
        on_source_added (r, l->data, self);
      g_list_free_full (sources, g_object_unref);
    }
}

/***
****  GObject virtual funcs
***/

static void
my_dispose (GObject * o)
{
  IndicatorDatetimePlannerEds * self = INDICATOR_DATETIME_PLANNER_EDS (o);
  priv_t * p = self->priv;

  if (p->cancellable != NULL)
    {
      g_cancellable_cancel (p->cancellable);
      g_clear_object (&p->cancellable);
    }

  if (p->source_registry != NULL)
    {
      g_signal_handlers_disconnect_by_func (p->source_registry,
                                            indicator_datetime_planner_emit_appointments_changed,
                                            self);

      g_clear_object (&self->priv->source_registry);
    }

  G_OBJECT_CLASS (indicator_datetime_planner_eds_parent_class)->dispose (o);
}

/***
****  Instantiation
***/

static void
indicator_datetime_planner_eds_class_init (IndicatorDatetimePlannerEdsClass * klass)
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

  g_type_class_add_private (klass, sizeof (IndicatorDatetimePlannerEdsPriv));
}

static void
indicator_datetime_planner_eds_init (IndicatorDatetimePlannerEds * self)
{
  priv_t * p;

  p = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                   INDICATOR_TYPE_DATETIME_PLANNER_EDS,
                                   IndicatorDatetimePlannerEdsPriv);

  self->priv = p;

  p->cancellable = g_cancellable_new ();

  e_source_registry_new (p->cancellable,
                         on_source_registry_ready,
                         self);
}

/***
****  Public
***/

IndicatorDatetimePlanner *
indicator_datetime_planner_eds_new (void)
{
  gpointer o = g_object_new (INDICATOR_TYPE_DATETIME_PLANNER_EDS, NULL);

  return INDICATOR_DATETIME_PLANNER (o);
}
