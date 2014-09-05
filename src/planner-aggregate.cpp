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

#include <datetime/planner-aggregate.h>

namespace unity {
namespace indicator {
namespace datetime {

/***
****
***/

class AggregatePlanner::Impl
{
public:
    Impl(AggregatePlanner* owner):
        m_owner(owner)
    {
    }

    ~Impl() =default;

    core::Property<std::vector<Appointment>>& appointments()
    {
        return m_appointments;
    }

    void add(const std::shared_ptr<Planner>& planner)
    {
        m_planners.push_back(planner);

        auto on_changed = [this](const std::vector<Appointment>&){rebuild();};
        auto connection = planner->appointments().changed().connect(on_changed);
        m_connections.push_back(connection);
    }

private:

    void rebuild()
    {
      // use a sorted aggregate vector of all our planners
      std::vector<Appointment> all;
      for (const auto& planner : m_planners) {
          const auto& walk = planner->appointments().get();
          all.insert(std::end(all), std::begin(walk), std::end(walk));
      }
      m_owner->sort(all);
      m_appointments.set(all);
    }

    const AggregatePlanner* m_owner = nullptr;
    core::Property<std::vector<Appointment>> m_appointments;
    std::vector<std::shared_ptr<Planner>> m_planners;
    std::vector<core::ScopedConnection> m_connections;
};

/***
****
***/

AggregatePlanner::AggregatePlanner():
  impl(new Impl{this})
{
}

AggregatePlanner::~AggregatePlanner()
{
}

core::Property<std::vector<Appointment>>&
AggregatePlanner::appointments()
{
    return impl->appointments();
}

void
AggregatePlanner::add(const std::shared_ptr<Planner>& planner)
{
    return impl->add(planner);
}

/***
****
***/

} // namespace datetime
} // namespace indicator
} // namespace unity

