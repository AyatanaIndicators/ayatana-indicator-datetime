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

namespace unity {
namespace indicator {
namespace datetime {

class LiveClock::Impl
{
public:

    Impl(LiveClock& owner, const std::shared_ptr<Timezones>& tzd):
        owner_(owner),
        timezones_(tzd)
    {
        if (timezones_)
        {
            timezones_->timezone.changed().connect ([this](const std::string& z) {setTimezone(z);});
            setTimezone(timezones_->timezone.get());
        }

        owner_.skewTestIntervalSec.changed().connect([this](unsigned int intervalSec) {setInterval(intervalSec);});
        setInterval(owner_.skewTestIntervalSec.get());
    }

    ~Impl()
    {
        clearTimer();

        g_clear_pointer (&timezone_, g_time_zone_unref);
    }

    GDateTime* localtime() const
    {
        g_assert (timezone_ != nullptr);

        return g_date_time_new_now (timezone_);
    }

private:

    void setTimezone (const std::string& str)
    {
        g_clear_pointer (&timezone_, g_time_zone_unref);
        timezone_= g_time_zone_new (str.c_str());
        owner_.skewDetected();
    }

private:

    void clearTimer()
    {
        if (skew_timeout_id_)
        {
            g_source_remove(skew_timeout_id_);
            skew_timeout_id_ = 0;
        }

        g_clear_pointer(&prev_datetime_, g_date_time_unref);
    }

    void setInterval(unsigned int seconds)
    {
        clearTimer();

        if (seconds > 0)
        {
            prev_datetime_ = owner_.localtime();
            skew_timeout_id_ = g_timeout_add_seconds(seconds, onTimerPulse, this);
        }
    }

    static gboolean onTimerPulse(gpointer gself)
    {
        auto self = static_cast<Impl*>(gself);

        // check to see if too much time passed since the last check */
        GDateTime * now = self->owner_.localtime();
        const GTimeSpan diff = g_date_time_difference(now, self->prev_datetime_);
        const GTimeSpan fuzz = 5;
        const GTimeSpan max = (self->owner_.skewTestIntervalSec.get() + fuzz) * G_USEC_PER_SEC;
        if (abs(diff) > max)
            self->owner_.skewDetected();

        // update prev_datetime
        g_clear_pointer(&self->prev_datetime_, g_date_time_unref);
        self->prev_datetime_ = now;

        return G_SOURCE_CONTINUE;
    }

protected:

    LiveClock& owner_;
    GTimeZone * timezone_ = nullptr;
    std::shared_ptr<Timezones> timezones_;

    GDateTime * prev_datetime_ = nullptr;
    unsigned int skew_timeout_id_ = 0;
    unsigned int sleep_subscription_id_ = 0;
};

LiveClock::LiveClock(const std::shared_ptr<Timezones>& tzd):
  p (new Impl (*this, tzd))
{
}

LiveClock::~LiveClock() =default;

GDateTime *
LiveClock::localtime() const
{
    return p->localtime();
}

} // namespace datetime
} // namespace indicator
} // namespace unity

