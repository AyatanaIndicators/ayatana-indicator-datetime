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

#include <datetime/clock.h>
#include <datetime/timezones.h>

namespace unity {
namespace indicator {
namespace datetime {

class LiveClock::Impl
{
public:

    Impl(LiveClock& owner, const std::shared_ptr<Timezones>& tzd):
        m_owner(owner),
        m_timezones(tzd)
    {
        if (m_timezones)
        {
            m_timezones->timezone.changed().connect([this](const std::string& z) {setTimezone(z);});
            setTimezone(m_timezones->timezone.get());
        }

        m_owner.skewTestIntervalSec.changed().connect([this](unsigned int intervalSec) {setInterval(intervalSec);});
        setInterval(m_owner.skewTestIntervalSec.get());
    }

    ~Impl()
    {
        clearTimer();

        g_clear_pointer(&m_timezone, g_time_zone_unref);
    }

    DateTime localtime() const
    {
        g_assert(m_timezone != nullptr);

        auto gdt = g_date_time_new_now(m_timezone);
        DateTime ret(gdt);
        g_date_time_unref(gdt);
        return ret;
    }

private:

    void setTimezone(const std::string& str)
    {
        g_clear_pointer(&m_timezone, g_time_zone_unref);
        m_timezone= g_time_zone_new(str.c_str());
        m_owner.skewDetected();
    }

private:

    void clearTimer()
    {
        if (m_skew_timeout_id)
        {
            g_source_remove(m_skew_timeout_id);
            m_skew_timeout_id = 0;
        }

        m_prev_datetime.reset();
    }

    void setInterval(unsigned int seconds)
    {
        clearTimer();

        if (seconds > 0)
        {
            m_prev_datetime = localtime();
            m_skew_timeout_id = g_timeout_add_seconds(seconds, onTimerPulse, this);
        }
    }

    static gboolean onTimerPulse(gpointer gself)
    {
        static_cast<Impl*>(gself)->onTimerPulse();
        return G_SOURCE_CONTINUE;
    }

    void onTimerPulse()
    {
        // check to see if too much time passed since the last check */
        const auto now = localtime();
        const auto diff = now.difference (m_prev_datetime);
        const GTimeSpan fuzz = 5;
        const GTimeSpan max = (m_owner.skewTestIntervalSec.get() + fuzz) * G_USEC_PER_SEC;
        if (abs(diff) > max)
            m_owner.skewDetected();

        // check to see if the day has changed
        if (now.day_of_year() != m_prev_datetime.day_of_year())
            m_owner.dateChanged();

        // update m_prev_datetime
        m_prev_datetime = now;
    }

protected:

    LiveClock& m_owner;
    GTimeZone * m_timezone = nullptr;
    std::shared_ptr<Timezones> m_timezones;

    DateTime m_prev_datetime;
    unsigned int m_skew_timeout_id = 0;
};

LiveClock::LiveClock(const std::shared_ptr<Timezones>& tzd):
  p(new Impl(*this, tzd))
{
}

LiveClock::~LiveClock() =default;

DateTime LiveClock::localtime() const
{
    return p->localtime();
}

} // namespace datetime
} // namespace indicator
} // namespace unity

