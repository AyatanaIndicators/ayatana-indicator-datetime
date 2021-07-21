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

#include <datetime/planner-snooze.h>

#include <libedataserver/libedataserver.h> // e_uid_new()

namespace ayatana {
namespace indicator {
namespace datetime {

/***
****
***/

class SnoozePlanner::Impl
{
public:

    Impl(SnoozePlanner* owner,
         const std::shared_ptr<Settings>& settings,
         const std::shared_ptr<Clock>& clock):
                m_owner(owner),
                m_settings(settings),
                m_clock(clock)
    {
    }

    ~Impl()
    {
    }

    core::Property<std::vector<Appointment>>& appointments()
    {
        return m_appointments;
    }

    void add(const Appointment& appt_in, const Alarm& alarm)
    {
        // make a copy of the appointment with only this alarm
        Appointment appt = appt_in;
        appt.alarms.clear();
        appt.alarms.push_back(alarm);

        // reschedule the alarm to go off N minutes from now
        // also take into count every whole minute since the alarm went off
        const auto offset_to_now = std::chrono::duration_cast<std::chrono::minutes>(std::chrono::microseconds(DateTime::NowLocal() - appt.begin));
        const auto offset = offset_to_now + std::chrono::minutes(m_settings->snooze_duration.get());
        appt.begin += offset;
        appt.end += offset;
        appt.alarms[0].time += offset;

        // give it a new ID
        gchar* uid = e_uid_new();
        appt.uid = uid;
        g_free(uid);

        // add it to our appointment list
        auto tmp = appointments().get();
        tmp.push_back(appt);
        m_owner->sort(tmp);
        m_appointments.set(tmp);
    }

private:

    const SnoozePlanner* const m_owner;
    const std::shared_ptr<Settings> m_settings;
    const std::shared_ptr<Clock> m_clock;
    core::Property<std::vector<Appointment>> m_appointments;
};

/***
****
***/

SnoozePlanner::SnoozePlanner(const std::shared_ptr<Settings>& settings,
                             const std::shared_ptr<Clock>& clock):
  impl(new Impl{this, settings, clock})
{
}

SnoozePlanner::~SnoozePlanner()
{
}

void
SnoozePlanner::add(const Appointment& appointment, const Alarm& alarm)
{
    impl->add(appointment, alarm);
}

core::Property<std::vector<Appointment>>&
SnoozePlanner::appointments()
{
    return impl->appointments();
}

/***
****
***/

} // namespace datetime
} // namespace indicator
} // namespace ayatana
