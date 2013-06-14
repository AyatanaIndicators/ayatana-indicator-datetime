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

#include <gio/gio.h> /* GFile, GFileMonitor */

#include <libical/ical.h>
#include <libical/icaltime.h>
#include <libecal/libecal.h>
#include <libedataserver/libedataserver.h>

#include "planner-eds.h"

struct _IndicatorDatetimePlannerEdsPriv
{
  ESourceRegistry * source_registry;
};

typedef IndicatorDatetimePlannerEdsPriv priv_t;

G_DEFINE_TYPE (IndicatorDatetimePlannerEds,
               indicator_datetime_planner_eds,
               INDICATOR_TYPE_DATETIME_PLANNER)

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
      g_free (appt);
    }
}

/***
**** my_get_appointments() helpers
***/

struct my_get_appointments_data
{
  ESource * source;
  GSList * appointments;
};

static gboolean
my_get_appointments_foreach (ECalComponent * component,
                             time_t          begin,
                             time_t          end,
                             gpointer        gdata)
{
  const ECalComponentVType vtype = e_cal_component_get_vtype (component);
  struct my_get_appointments_data * data = gdata;

  if ((vtype == E_CAL_COMPONENT_EVENT) || (vtype == E_CAL_COMPONENT_TODO))
    {
      icalproperty_status status;
      e_cal_component_get_status (component, &status);
      if ((status != ICAL_STATUS_COMPLETED) && (status != ICAL_STATUS_CANCELLED))
        {
          ECalComponentText text;
          struct IndicatorDatetimeAppt * appt = g_new0 (struct IndicatorDatetimeAppt, 1);

          text.value = "";
          e_cal_component_get_summary (component, &text);
 
          appt->begin = g_date_time_new_from_unix_local (begin);
          appt->end = g_date_time_new_from_unix_local (end);
          appt->color = e_source_selectable_dup_color (e_source_get_extension (data->source, E_SOURCE_EXTENSION_CALENDAR));
          appt->is_event = vtype == E_CAL_COMPONENT_EVENT;
          appt->summary = g_strdup (text.value);

          data->appointments = g_slist_prepend (data->appointments, appt);
        }
    }

  return G_SOURCE_CONTINUE;
}


/***
****  IndicatorDatetimePlanner virtual funcs
***/

static GSList *
my_get_appointments (IndicatorDatetimePlanner  * planner,
                     GDateTime                 * begin_datetime,
                     GDateTime                 * end_datetime)
{
  GList * l;
  GList * sources;
  priv_t * p;
  const char * str;
  icaltimezone * default_timezone;
  struct my_get_appointments_data data;
  const int64_t begin = g_date_time_to_unix (begin_datetime);
  const int64_t end = g_date_time_to_unix (end_datetime);

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

  data.source = NULL;
  data.appointments = NULL;

  sources = e_source_registry_list_sources (p->source_registry, E_SOURCE_EXTENSION_CALENDAR);
  for (l=sources; l!=NULL; l=l->next)
    {
      GError * err;
      ESource * source;
      ECalClient * ecc;

      source = E_SOURCE (l->data);
      if (e_source_get_enabled (source))
        {
          err = NULL;
          ecc = e_cal_client_new (source, E_CAL_CLIENT_SOURCE_TYPE_EVENTS, &err);
          if (err != NULL)
            {
              g_warning ("Can't create ecal client: %s", err->message);
              g_error_free (err);
            }
          else
            {
              if (!e_client_open_sync (E_CLIENT (ecc), TRUE, NULL, &err))
                {
                  g_debug ("Failed to open ecal client: %s", err->message);
                  g_error_free (err);
                }
              else
                {
                  if (default_timezone != NULL)
                    e_cal_client_set_default_timezone (ecc, default_timezone);

                  data.source = source;
                  e_cal_client_generate_instances_sync (ecc, begin, end, my_get_appointments_foreach, &data);
                }

              g_object_unref (ecc);
            }
        }
    }

  g_list_free_full (sources, g_object_unref);

  g_debug ("%s EDS get_appointments returning %d appointments", G_STRLOC, g_slist_length (data.appointments));
  return data.appointments;
}

gboolean
my_is_configured (IndicatorDatetimePlanner * planner)
{
  GList * sources;
  gboolean have_sources;
  IndicatorDatetimePlannerEds * self;

  /* confirm that it's installed... */
  gchar *evo = g_find_program_in_path ("evolution");
  if (evo == NULL)
    return FALSE;

  g_debug ("found calendar app: '%s'", evo);
  g_free (evo);

  /* see if there are any calendar sources */
  self = INDICATOR_DATETIME_PLANNER_EDS (planner);
  sources = e_source_registry_list_sources (self->priv->source_registry, E_SOURCE_EXTENSION_CALENDAR);
  have_sources = sources != NULL;
  g_list_free_full (sources, g_object_unref);
  return have_sources;
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
****  GObject virtual funcs
***/

static void
my_dispose (GObject * o)
{
  IndicatorDatetimePlannerEds * self = INDICATOR_DATETIME_PLANNER_EDS (o);
  priv_t * p = self->priv;

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
****  Insantiation
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

  g_type_class_add_private (klass, sizeof (IndicatorDatetimePlannerEdsPriv));
}

static void
indicator_datetime_planner_eds_init (IndicatorDatetimePlannerEds * self)
{
  priv_t * p;
  GError * err;

  p = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                   INDICATOR_TYPE_DATETIME_PLANNER_EDS,
                                   IndicatorDatetimePlannerEdsPriv);

  self->priv = p;

  err = 0;
  p->source_registry = e_source_registry_new_sync (NULL, &err);
  if (err != NULL)
    {
      g_warning ("indicator-datetime cannot show EDS appointments: %s", err->message);
      g_error_free (err);
    }
  else
    {
      gpointer o = p->source_registry;
      g_signal_connect_swapped (o, "source-added",    G_CALLBACK(indicator_datetime_planner_emit_appointments_changed), self);
      g_signal_connect_swapped (o, "source-removed",  G_CALLBACK(indicator_datetime_planner_emit_appointments_changed), self);
      g_signal_connect_swapped (o, "source-changed",  G_CALLBACK(indicator_datetime_planner_emit_appointments_changed), self);
      g_signal_connect_swapped (o, "source-disabled", G_CALLBACK(indicator_datetime_planner_emit_appointments_changed), self);
      g_signal_connect_swapped (o, "source-enabled",  G_CALLBACK(indicator_datetime_planner_emit_appointments_changed), self);
    }
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
