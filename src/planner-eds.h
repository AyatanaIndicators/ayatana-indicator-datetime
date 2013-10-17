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

#ifndef __INDICATOR_DATETIME_PLANNER_EDS__H__
#define __INDICATOR_DATETIME_PLANNER_EDS__H__

#include "planner.h" /* parent class */

G_BEGIN_DECLS

#define INDICATOR_TYPE_DATETIME_PLANNER_EDS          (indicator_datetime_planner_eds_get_type())
#define INDICATOR_DATETIME_PLANNER_EDS(o)            (G_TYPE_CHECK_INSTANCE_CAST ((o), INDICATOR_TYPE_DATETIME_PLANNER_EDS, IndicatorDatetimePlannerEds))
#define INDICATOR_DATETIME_PLANNER_EDS_GET_CLASS(o)  (G_TYPE_INSTANCE_GET_CLASS ((o), INDICATOR_TYPE_DATETIME_PLANNER_EDS, IndicatorDatetimePlannerEdsClass))
#define INDICATOR_IS_DATETIME_PLANNER_EDS(o)         (G_TYPE_CHECK_INSTANCE_TYPE ((o), INDICATOR_TYPE_DATETIME_PLANNER_EDS))

typedef struct _IndicatorDatetimePlannerEds        IndicatorDatetimePlannerEds;
typedef struct _IndicatorDatetimePlannerEdsPriv    IndicatorDatetimePlannerEdsPriv;
typedef struct _IndicatorDatetimePlannerEdsClass   IndicatorDatetimePlannerEdsClass;

GType indicator_datetime_planner_eds_get_type (void);

/**
 * An IndicatorDatetimePlanner which uses Evolution Data Server
 * to get its list of appointments.
 */
struct _IndicatorDatetimePlannerEds
{
  /*< private >*/
  IndicatorDatetimePlanner parent;
  IndicatorDatetimePlannerEdsPriv * priv;
};

struct _IndicatorDatetimePlannerEdsClass
{
  IndicatorDatetimePlannerClass parent_class;
};

IndicatorDatetimePlanner * indicator_datetime_planner_eds_new (void);

G_END_DECLS

#endif /* __INDICATOR_DATETIME_PLANNER_EDS__H__ */
