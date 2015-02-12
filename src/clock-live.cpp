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
#include <datetime/timezone.h>

#include <glib-unix.h> // g_unix_fd_add()

#include <sys/timerfd.h>
#include <unistd.h> // close()

#ifndef TFD_TIMER_CANCEL_ON_SET
 #define TFD_TIMER_CANCEL_ON_SET (1 << 1)
#endif

namespace unity {
namespace indicator {
namespace datetime {

/***
****
***/

class LiveClock::Impl
{
public:

    Impl(LiveClock& owner, const std::shared_ptr<const Timezone>& timezone_):
        m_owner(owner),
        m_timezone(timezone_)
    {
        if (m_timezone)
        {
            auto setter = [this](const std::string& z){setTimezone(z);};
            m_timezone->timezone.changed().connect(setter);
            setter(m_timezone->timezone.get());
        }

        m_timerfd = timerfd_create(CLOCK_REALTIME, 0);
        if (m_timerfd == -1)
        {
            g_warning("unable to create realtime timer: %s", g_strerror(errno));
        }
        else
        {
            struct itimerspec timerval;
            // set args to fire at the beginning of the next minute...
            int flags = TFD_TIMER_ABSTIME;
            auto now = g_date_time_new_now(m_gtimezone);
            auto next = g_date_time_add_minutes(now, 1);
            auto start_of_next = g_date_time_add_seconds(next, -g_date_time_get_seconds(next));
            timerval.it_value.tv_sec = g_date_time_to_unix(start_of_next);
            timerval.it_value.tv_nsec = 0;
            g_date_time_unref(start_of_next);
            g_date_time_unref(next);
            g_date_time_unref(now);
            // ...and also to fire at the beginning of every subsequent minute...
            timerval.it_interval.tv_sec = 60;
            timerval.it_interval.tv_nsec = 0;
            // ...and also to fire if someone changes the time
            // manually (eg toggling from manual<->ntp)
            flags |= TFD_TIMER_CANCEL_ON_SET;

            if (timerfd_settime(m_timerfd, flags, &timerval, NULL) == -1)
                g_error("timerfd_settime failed: %s", g_strerror(errno));

            m_timerfd_tag = g_unix_fd_add(m_timerfd,
                                          (GIOCondition)(G_IO_IN|G_IO_HUP|G_IO_ERR),
                                          on_timerfd_cond,
                                          this);
        }

        refresh();
    }

    ~Impl()
    {
        if (m_timerfd_tag != 0)
            g_source_remove(m_timerfd_tag);

        if (m_timerfd != -1)
            close(m_timerfd);

        g_clear_pointer(&m_gtimezone, g_time_zone_unref);
    }

    DateTime localtime() const
    {
        g_assert(m_gtimezone != nullptr);

        auto gdt = g_date_time_new_now(m_gtimezone);
        DateTime ret(gdt);
        g_date_time_unref(gdt);
        return ret;
    }

private:

    static gboolean on_timerfd_cond (gint fd, GIOCondition cond G_GNUC_UNUSED, gpointer gself)
    {
        // let's see what triggered this event
        auto self = static_cast<Impl*>(gself);
        uint64_t n_interrupts = 0;
        auto s = read(fd, &n_interrupts, sizeof(uint64_t));

        // make a debug log of what just happened
        auto now = g_date_time_new_now(self->m_gtimezone);
        auto now_str = g_date_time_format(now, "%F %T");
        g_debug("%s at %s (%f), read %zd bytes to get n_interrupts %zu",
                G_STRFUNC, now_str, g_date_time_get_seconds(now),
                s, n_interrupts);
        g_free(now_str);
        g_date_time_unref(now);
  
        self->refresh();
        return G_SOURCE_CONTINUE;
    }

    /***
    ****
    ***/

    void setTimezone(const std::string& str)
    {
        g_clear_pointer(&m_gtimezone, g_time_zone_unref);
        m_gtimezone = g_time_zone_new(str.c_str());
        m_owner.minute_changed();
    }

    /***
    ****
    ***/

    void refresh()
    {
        const auto now = localtime();

        // maybe emit change signals
        if (!DateTime::is_same_minute(m_prev_datetime, now))
            m_owner.minute_changed();
        if (!DateTime::is_same_day(m_prev_datetime, now))
            m_owner.date_changed();

        m_prev_datetime = now;
    }

protected:

    LiveClock& m_owner;
    GTimeZone* m_gtimezone = nullptr;
    std::shared_ptr<const Timezone> m_timezone;

    DateTime m_prev_datetime;
    int m_timerfd = -1;
    guint m_timerfd_tag = 0;
};

LiveClock::LiveClock(const std::shared_ptr<const Timezone>& timezone_):
    p(new Impl(*this, timezone_))
{
}

LiveClock::~LiveClock() =default;

DateTime LiveClock::localtime() const
{
    return p->localtime();
}

/***
****
***/

} // namespace datetime
} // namespace indicator
} // namespace unity

