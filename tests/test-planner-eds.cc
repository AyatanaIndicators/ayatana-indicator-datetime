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

#include "clock-mock.h"
#include "glib-fixture.h"

#include <datetime/planner-eds.h>

#include <glib/gi18n.h>

#include <langinfo.h>
#include <locale.h>

using unity::indicator::datetime::Appointment;
using unity::indicator::datetime::DateTime;
using unity::indicator::datetime::PlannerEds;

/***
****
***/

class PlannerEdsFixture: public GlibFixture
{
  private:

    typedef GlibFixture super;

  protected:

    virtual void SetUp ()
    {
      super::SetUp ();
    }

    virtual void TearDown ()
    {
      super::TearDown ();
    }
};

/***
****
***/

TEST_F (PlannerEdsFixture, HelloWorld)
{
    DateTime dt;
    dt = g_date_time_new_now_local();

    PlannerEds planner;
    planner.time.set (dt);
    g_main_loop_run (loop);
}

