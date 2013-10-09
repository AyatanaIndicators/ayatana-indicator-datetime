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

#ifndef __INDICATOR_DATETIME_PLANNER__H__
#define __INDICATOR_DATETIME_PLANNER__H__

#include <glib.h>
#include <glib-object.h> /* parent class */
#include <gio/gio.h>

G_BEGIN_DECLS

#define INDICATOR_TYPE_DATETIME_PLANNER          (indicator_datetime_planner_get_type())
#define INDICATOR_DATETIME_PLANNER(o)            (G_TYPE_CHECK_INSTANCE_CAST ((o), INDICATOR_TYPE_DATETIME_PLANNER, IndicatorDatetimePlanner))
#define INDICATOR_DATETIME_PLANNER_GET_CLASS(o)  (G_TYPE_INSTANCE_GET_CLASS ((o), INDICATOR_TYPE_DATETIME_PLANNER, IndicatorDatetimePlannerClass))
#define INDICATOR_DATETIME_PLANNER_CLASS(k)      (G_TYPE_CHECK_CLASS_CAST ((k), INDICATOR_TYPE_DATETIME_PLANNER, IndicatorDatetimePlannerClass))
#define INDICATOR_IS_DATETIME_PLANNER(o)         (G_TYPE_CHECK_INSTANCE_TYPE ((o), INDICATOR_TYPE_DATETIME_PLANNER))

typedef struct _IndicatorDatetimePlanner        IndicatorDatetimePlanner;
typedef struct _IndicatorDatetimePlannerPriv    IndicatorDatetimePlannerPriv;
typedef struct _IndicatorDatetimePlannerClass   IndicatorDatetimePlannerClass;

GType indicator_datetime_planner_get_type (void);

struct IndicatorDatetimeAppt
{
  char * color;
  char * summary;
  char * url;
  GDateTime * begin;
  GDateTime * end;
  gboolean is_event;
  gboolean is_daily;
  gboolean has_alarms;
};

/**
 * Abstract Base Class for objects that provides appointments and events.
 *
 * These will be listed in the appointments section of indicator-datetime's menu.
 */
struct _IndicatorDatetimePlanner
{
  /*< private >*/
  GObject parent;
  IndicatorDatetimePlannerPriv * priv;
};

struct _IndicatorDatetimePlannerClass
{
  GObjectClass parent_class;

  /* signals */

  void (*appointments_changed)  (IndicatorDatetimePlanner * self);

  /* virtual functions */

  void (*get_appointments)      (IndicatorDatetimePlanner * self,
                                 GDateTime                * begin,
                                 GDateTime                * end,
                                 GAsyncReadyCallback        callback,
                                 gpointer                   user_data);

  GSList* (*get_appointments_finish) (IndicatorDatetimePlanner  * self,
                                      GAsyncResult              * res,       
                                      GError                   ** error);        


  gboolean (*is_configured)     (IndicatorDatetimePlanner * self);
  void (*activate)              (IndicatorDatetimePlanner * self);
  void (*activate_time)         (IndicatorDatetimePlanner * self, GDateTime *);
};

/***
****
***/

void indicator_datetime_appt_free (struct IndicatorDatetimeAppt * appt);

/**
 * Get a list of appointments, sorted by start time.
 */
void indicator_datetime_planner_get_appointments (IndicatorDatetimePlanner * self,
                                                  GDateTime                * begin,
                                                  GDateTime                * end,
                                                  GAsyncReadyCallback        callback,
                                                  gpointer                   user_data);

/**
 * Finishes the async call begun with indicator_datetime_planner_get_appointments()
 *
 * To free the list properly, use indicator_datetime_planner_free_appointments()
 * 
 * Return value: (element-type IndicatorDatetimeAppt)
 *               (transfer full):
 *               list of appointments
 */
GSList * indicator_datetime_planner_get_appointments_finish (IndicatorDatetimePlanner  * self,
                                                             GAsyncResult              * res,
                                                             GError                   ** error);

/**
 * Convenience function for freeing a GSList of IndicatorDatetimeAppt.
 *
 * Equivalent to g_slist_free_full (list, (GDestroyNotify)indicator_datetime_appt_free);
 */
void indicator_datetime_planner_free_appointments (GSList *);


/**
 * Returns false if the planner's backend is not configured.
 *
 * This can be used on startup to determine whether or not to use this planner.
 */
gboolean indicator_datetime_planner_is_configured (IndicatorDatetimePlanner * self);

/**
 * Activate this planner.
 *
 * This is used to activate the planner's backend's event editor.
 */
void indicator_datetime_planner_activate (IndicatorDatetimePlanner * self);

/**
 * Activate this planner.
 *
 * This is used to activate the planner's backend's event editor,
 * with an added hint of the specific time that the user would like to edit.
 */
void indicator_datetime_planner_activate_time (IndicatorDatetimePlanner * self, GDateTime * time);

/**
 * Set the timezone.
 *
 * This is used as a default timezone if the backend's events don't provide their own.
 */
void indicator_datetime_planner_set_timezone (IndicatorDatetimePlanner * self, const char * timezone);

const char * indicator_datetime_planner_get_timezone (IndicatorDatetimePlanner * self);


/**
 * Emits the "appointments-changed" signal. This should only be called by subclasses.
 */
void indicator_datetime_planner_emit_appointments_changed (IndicatorDatetimePlanner * self);

G_END_DECLS

#endif /* __INDICATOR_DATETIME_PLANNER__H__ */
