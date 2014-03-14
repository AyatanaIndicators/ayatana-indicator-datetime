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

#include <datetime/planner-eds.h>
#include <datetime/engine-eds.h>

namespace unity {
namespace indicator {
namespace datetime {

/***
****
***/

EdsPlanner::EdsPlanner(const std::shared_ptr<EdsEngine>& engine,
                       const std::shared_ptr<Timezone>& timezone):
    m_engine(engine),
    m_timezone(timezone)
{
    m_engine->changed().connect([this](){
        g_debug("EdsPlanner %p rebuilding soon because EdsEngine %p emitted 'changed' signal%p", this, m_engine.get());
        rebuild_soon();
    });
}

EdsPlanner::~EdsPlanner() =default;

void EdsPlanner::rebuild_now()
{
    const auto& r = range().get();

    auto on_appointments_fetched = [this](const std::vector<Appointment>& a){
        g_debug("EdsPlanner %p got %zu appointments", this, a.size());
        m_appointments.set(a);
    };

    m_engine->get_appointments(r.first, r.second, *m_timezone.get(), on_appointments_fetched);
}

core::Property<std::vector<Appointment>>& EdsPlanner::appointments()
{
    return m_appointments;
}

/***
****
***/

} // namespace datetime
} // namespace indicator
} // namespace unity
