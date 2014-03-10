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

#include <datetime/planner-range.h>

namespace unity {
namespace indicator {
namespace datetime {

/***
****
***/

RangePlanner::RangePlanner():
    m_range(std::pair<DateTime,DateTime>(DateTime::NowLocal(), DateTime::NowLocal()))
{
    range().changed().connect([this](const std::pair<DateTime,DateTime>&){
        g_debug("rebuilding because the date range changed");
        rebuild_soon();
    });
}

RangePlanner::~RangePlanner()
{
    if (m_rebuild_tag)
        g_source_remove(m_rebuild_tag);
}

void RangePlanner::rebuild_soon()
{
    static const int ARBITRARY_BATCH_MSEC = 200;

    if (m_rebuild_tag == 0)
        m_rebuild_tag = g_timeout_add(ARBITRARY_BATCH_MSEC, rebuild_now_static, this);
}

gboolean RangePlanner::rebuild_now_static(gpointer gself)
{
    auto self = static_cast<RangePlanner*>(gself);
    self->m_rebuild_tag = 0;
    self->rebuild_now();
    return G_SOURCE_REMOVE;
}

core::Property<std::pair<DateTime,DateTime>>& RangePlanner::range()
{
    return m_range;
}

/***
****
***/

} // namespace datetime
} // namespace indicator
} // namespace unity
