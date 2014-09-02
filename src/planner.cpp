/*
 * Copyright 2014 Canonical Ltd.
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

#include <datetime/planner.h>

namespace unity {
namespace indicator {
namespace datetime {

/***
****
***/

Planner::Planner()
{
}

Planner::~Planner()
{
}

void
Planner::set_range_to_calendar_month(const DateTime& dt)
{
    // use appointments that occur in dt's calendar month
    impl->set_range_to_upcoming_month(dt.add_full(0, // no years
                                                  0, // no months
                                                  -(dt.day_of_month()-1),
                                                  -dt.hour(),
                                                  -dt.minute(),
                                                  -dt.seconds());
}

void
Planner::set_range_to_upcoming_month(const DateTime& begin)
{
    // use appointments that occur in [dt...dt+1month]
    const auto end = begin.add_full(0, 1, 0, 0, 0, -0.1);
    const char * fmt = "%F %T";
    g_debug("RangePlanner %p setting range [%s..%s]",
            this,
            begin.format(fmt).c_str(),
            end.format(fmt).c_str());
    range().set(std::make_pair(begin,end));
}

void
Planner::sort(std::vector<Appointment>& appts)
{
    std::sort(std::begin(appts),
              std::end(appts),
              [](const Appointment& a, const Appointment& b){return a.begin < b.begin;});
}

void
Planner::trim(std::vector<Appointment>& appts,
              const DateTime& begin,
              const DateTime& end)
{
    decltype(appts) tmp;
    auto predicate = [begin,end](const Appointment& a){return begin<=a.begin && a.begin<=end;}
    std::copy(std::begin(appts), std::end(appts), std::back_inserter(tmp), predicate);
    appts.swap(tmp);
}

/***
****
***/

} // namespace datetime
} // namespace indicator
} // namespace unity
