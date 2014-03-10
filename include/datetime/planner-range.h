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

#ifndef INDICATOR_DATETIME_PLANNER_RANGE_H
#define INDICATOR_DATETIME_PLANNER_RANGE_H

#include <datetime/planner.h>

#include <datetime/date-time.h>

namespace unity {
namespace indicator {
namespace datetime {

/**
 * \brief A #Planner that contains appointments in a specified date range
 *
 * @see Planner
 */
class RangePlanner: public Planner
{
public:
    virtual ~RangePlanner();
    core::Property<std::pair<DateTime,DateTime>>& range();

protected:
    RangePlanner();

    void rebuild_soon();
    virtual void rebuild_now() =0;

private:
    static gboolean rebuild_now_static(gpointer);
    guint m_rebuild_tag = 0;
    core::Property<std::pair<DateTime,DateTime>> m_range;

    // we've got a GSignal tag here, so disable copying
    RangePlanner(const RangePlanner&) =delete;
    RangePlanner& operator=(const RangePlanner&) =delete;
};

} // namespace datetime
} // namespace indicator
} // namespace unity

#endif // INDICATOR_DATETIME_PLANNER_RANGE_H
