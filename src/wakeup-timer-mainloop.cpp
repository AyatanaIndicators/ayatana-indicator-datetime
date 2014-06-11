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

#include <datetime/wakeup-timer-mainloop.h>

#include <glib.h>

#include <cstdlib> // abs()

namespace unity {
namespace indicator {
namespace datetime {

/***
****
***/

class MainloopWakeupTimer::Impl
{

public:

    Impl(const std::shared_ptr<Clock>& clock):
        m_clock(clock)
    {
    }

    ~Impl()
    {
        cancel_timer();
    }

    void set_wakeup_time(const DateTime& d)
    {
        m_wakeup_time = d;

        rebuild_timer();
    }

    core::Signal<>& timeout() { return m_timeout; }

private:

    void rebuild_timer()
    {
        cancel_timer();

        g_return_if_fail(m_wakeup_time.is_set());

        const auto now = m_clock->localtime();
        const auto difference_usec = g_date_time_difference(m_wakeup_time.get(), now.get());
        const guint interval_msec = std::abs(difference_usec) / 1000u;
        g_debug("%s setting wakeup timer to kick at %s, which is in %zu seconds",
                G_STRFUNC,
                m_wakeup_time.format("%F %T").c_str(),
                size_t{interval_msec/1000});

        m_timeout_tag = g_timeout_add_full(G_PRIORITY_HIGH,
                                           interval_msec,
                                           on_timeout,
                                           this,
                                           nullptr);
    }

    static gboolean on_timeout(gpointer gself)
    {
        g_debug("%s %s", G_STRLOC, G_STRFUNC);
        static_cast<Impl*>(gself)->on_timeout();
        return G_SOURCE_REMOVE;
    }

    void on_timeout()
    {
        cancel_timer();
        m_timeout();
    }

    void cancel_timer()
    {
        if (m_timeout_tag != 0)
        {
            g_source_remove(m_timeout_tag);
            m_timeout_tag = 0;
        }
    }

    core::Signal<> m_timeout;
    const std::shared_ptr<Clock>& m_clock;
    guint m_timeout_tag = 0;
    DateTime m_wakeup_time;
};

/***
****
***/

MainloopWakeupTimer::MainloopWakeupTimer(const std::shared_ptr<Clock>& clock):
    p(new Impl(clock))
{
}

MainloopWakeupTimer::~MainloopWakeupTimer()
{
}

void MainloopWakeupTimer::set_wakeup_time(const DateTime& d)
{
    p->set_wakeup_time(d);
}

core::Signal<>& MainloopWakeupTimer::timeout()
{
    return p->timeout();
}

/***
****
***/

} // namespace datetime
} // namespace indicator
} // namespace unity
