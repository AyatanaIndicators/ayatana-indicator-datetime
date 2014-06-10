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

#include <datetime/wakeup-timer-uha.h>

#include <ubuntu/hardware/alarm.h>

#include <glib.h>

#include <unistd.h>

#include <ctime> // struct timespec
#include <mutex>
#include <thread>

namespace unity {
namespace indicator {
namespace datetime {

/***
****
***/

class UhaWakeupTimer::Impl
{

public:

    Impl(const std::shared_ptr<Clock>& clock):
        m_clock(clock),
        m_hardware_alarm(u_hardware_alarm_create())
    {
        // fire up a worker thread that initially just sleeps
        set_wakeup_time_to_the_distant_future();
        m_thread = std::move(std::thread([&](){threadfunc();}));
    }

    ~Impl()
    {
        // tell the worker thread to wake up and exit
        m_yielding = true;
        set_wakeup_time(m_clock->localtime().add_full(0,0,0,0,0,0.1));

        // wait for it to happen
        if (m_thread.joinable())
            m_thread.join();

        g_idle_remove_by_data(this);

        u_hardware_alarm_unref(m_hardware_alarm);
    }

    void set_wakeup_time(const DateTime& d)
    {
        g_debug("%s %s", G_STRLOC, G_STRFUNC);
        std::lock_guard<std::recursive_mutex> lg(m_mutex);

        const auto wakeup_time = d.to_unix();

        // simple sanity check: don't try to wait for something that's already passed
        const auto now = m_clock->localtime().to_unix();
        g_return_if_fail (wakeup_time >= now);

        struct timespec sleep_interval;
        sleep_interval.tv_sec = wakeup_time;
        sleep_interval.tv_nsec = 0;
        g_debug("%s %s setting hardware wakeup time to %s (%zu seconds from now)",
                G_STRLOC, G_STRFUNC,
                d.format("%F %T").c_str(),
                (size_t)(wakeup_time - now));
        u_hardware_alarm_set_relative_to_with_behavior(m_hardware_alarm,
                                                       U_HARDWARE_ALARM_TIME_REFERENCE_RTC,
                                                       U_HARDWARE_ALARM_SLEEP_BEHAVIOR_WAKEUP_DEVICE,
                                                       &sleep_interval);
    }

    core::Signal<>& timeout() { return m_timeout; }

private:

    void set_wakeup_time_to_the_distant_future()
    {
        const auto tomorrow = m_clock->localtime().add_full(0,0,1,0,0,0);
        set_wakeup_time(tomorrow);
    }

    static gboolean kick_idle (gpointer gself)
    {
        static_cast<Impl*>(gself)->m_timeout();

        return G_SOURCE_REMOVE;
    }

    void threadfunc()
    {
        while (!m_yielding)
        {
            // wait for the next hw alarm
            UHardwareAlarmWaitResult wait_result;
            g_debug ("calling wait_for_next_alarm");
            auto rc = u_hardware_alarm_wait_for_next_alarm(m_hardware_alarm, &wait_result);
            g_return_if_fail (rc == U_STATUS_SUCCESS);

            // set a long wakeup interval for the next iteration of the loop.
            // if there's another Appointment queued up by the Planner,
            // our timeout() listener will call set_wakeup_time() to set the
            // real wakeup interval.
            set_wakeup_time_to_the_distant_future();

            // delegate the kick back to the main thread
            g_idle_add (kick_idle, this);
        }
    }

    core::Signal<> m_timeout;
    std::recursive_mutex m_mutex;
    bool m_yielding = false;
    const std::shared_ptr<Clock>& m_clock;
    UHardwareAlarm m_hardware_alarm = nullptr;
    std::thread m_thread;
};

/***
****
***/

UhaWakeupTimer::UhaWakeupTimer(const std::shared_ptr<Clock>& clock):
    p(new Impl(clock))
{
}

UhaWakeupTimer::~UhaWakeupTimer()
{
}

bool UhaWakeupTimer::is_supported()
{
    auto hw_alarm = u_hardware_alarm_create();
    g_debug ("%s hardware alarm %p", G_STRFUNC, hw_alarm);
    return hw_alarm != nullptr;
}

void UhaWakeupTimer::set_wakeup_time(const DateTime& d)
{
    p->set_wakeup_time(d);
}

core::Signal<>& UhaWakeupTimer::timeout()
{
    return p->timeout();
}

/***
****
***/

} // namespace datetime
} // namespace indicator
} // namespace unity
