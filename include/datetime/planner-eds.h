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

#ifndef INDICATOR_DATETIME_PLANNER_EDS_H
#define INDICATOR_DATETIME_PLANNER_EDS_H

#include <datetime/clock.h>
#include <datetime/planner.h>
#include <datetime/timezone.h>

#include <memory> // shared_ptr, unique_ptr

namespace unity {
namespace indicator {
namespace datetime {

/**
 * \brief Planner which uses EDS as its backend
 */
class PlannerEds: public Planner
{
public:
    PlannerEds(const std::shared_ptr<Clock>& clock,
               const std::shared_ptr<Timezone>& timezone);
    virtual ~PlannerEds();

private:
    class Impl;
    std::unique_ptr<Impl> p;
};

} // namespace datetime
} // namespace indicator
} // namespace unity

#endif // INDICATOR_DATETIME_PLANNER_EDS_H
