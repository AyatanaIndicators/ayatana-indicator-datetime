/*
 * Copyright 2013 Canonical Ltd.
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
 *
 * Authors:
 *   Charles Kerr <charles.kerr@canonical.com>
 */

#include <datetime/actions-live.h>

#include <glib.h>

namespace unity {
namespace indicator {
namespace datetime {

/***
****
***/

void LiveActions::open_desktop_settings()
{
    g_message ("%s", G_STRFUNC);
}

void LiveActions::open_phone_settings()
{
    g_message("%s", G_STRFUNC);
}

void LiveActions::open_phone_clock_app()
{
    g_message("%s", G_STRFUNC);
}

void LiveActions::open_planner()
{
    g_message("%s", G_STRFUNC);
}

void LiveActions::open_planner_at(const DateTime&)
{
    g_message("%s", G_STRFUNC);
}

void LiveActions::open_appointment(const std::string& uid)
{
    g_message("%s - %s", G_STRFUNC, uid.c_str());
}

void LiveActions::set_location(const std::string& zone, const std::string& name)
{
    g_message("%s - %s %s", G_STRFUNC, zone.c_str(), name.c_str());
}

void LiveActions::set_calendar_date(const DateTime&)
{
    g_message("%s", G_STRFUNC);
}


/***
****
***/

} // namespace datetime
} // namespace indicator
} // namespace unity
