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

#ifndef __INDICATOR_DATETIME_PLANNER_MOCK__H__
#define __INDICATOR_DATETIME_PLANNER_MOCK__H__

#include "planner.h" /* parent class */

G_BEGIN_DECLS

#define INDICATOR_TYPE_DATETIME_PLANNER_MOCK          (indicator_datetime_planner_mock_get_type())
#define INDICATOR_DATETIME_PLANNER_MOCK(o)            (G_TYPE_CHECK_INSTANCE_CAST ((o), INDICATOR_TYPE_DATETIME_PLANNER_MOCK, IndicatorDatetimePlannerMock))
#define INDICATOR_DATETIME_PLANNER_MOCK_GET_CLASS(o)  (G_TYPE_INSTANCE_GET_CLASS ((o), INDICATOR_TYPE_DATETIME_PLANNER_MOCK, IndicatorDatetimePlannerMockClass))
#define INDICATOR_IS_DATETIME_PLANNER_MOCK(o)         (G_TYPE_CHECK_INSTANCE_TYPE ((o), INDICATOR_TYPE_DATETIME_PLANNER_MOCK))

typedef struct _IndicatorDatetimePlannerMock        IndicatorDatetimePlannerMock;
typedef struct _IndicatorDatetimePlannerMockPriv    IndicatorDatetimePlannerMockPriv;
typedef struct _IndicatorDatetimePlannerMockClass   IndicatorDatetimePlannerMockClass;

GType indicator_datetime_planner_mock_get_type (void);

/**
 * An IndicatorDatetimePlanner which uses Evolution Data Server
 * to get its list of appointments.
 */
struct _IndicatorDatetimePlannerMock
{
  /*< private >*/
  IndicatorDatetimePlanner parent;
  IndicatorDatetimePlannerMockPriv * priv;
};

struct _IndicatorDatetimePlannerMockClass
{
  IndicatorDatetimePlannerClass parent_class;
};

IndicatorDatetimePlanner * indicator_datetime_planner_mock_new (void);

G_END_DECLS

#endif /* __INDICATOR_DATETIME_PLANNER_MOCK__H__ */
