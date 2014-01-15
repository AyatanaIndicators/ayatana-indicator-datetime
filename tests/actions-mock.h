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

#ifndef INDICATOR_DATETIME_ACTIONS_MOCK_H
#define INDICATOR_DATETIME_ACTIONS_MOCK_H

#include <datetime/actions.h>

#include <set>

namespace unity {
namespace indicator {
namespace datetime {

class MockActions: public Actions
{
public:
    MockActions(std::shared_ptr<State>& state_in): Actions(state_in) {}
    ~MockActions() =default;

    enum Action { OpenDesktopSettings, OpenPhoneSettings, OpenPhoneClockApp,
                  OpenPlanner, OpenPlannerAt, OpenAppointment,
                  SetLocation, SetCalendarDate };
    const std::vector<Action>& history() const { return m_history; }
    const DateTime& date_time() const { return m_date_time; }
    const std::string& zone() const { return m_zone; }
    const std::string& name() const { return m_name; }
    const std::string& url() const { return m_url; }
    void clear() { m_history.clear(); m_zone.clear(); m_name.clear(); }

    void open_desktop_settings() { m_history.push_back(OpenDesktopSettings); }

    void open_phone_settings() { m_history.push_back(OpenPhoneSettings); }

    void open_phone_clock_app() { m_history.push_back(OpenPhoneClockApp); }

    void open_planner() { m_history.push_back(OpenPlanner); }

    void open_planner_at(const DateTime& date_time_) {
        m_history.push_back(OpenPlannerAt);
        m_date_time = date_time_;
    }

    void set_location(const std::string& zone_, const std::string& name_) {
        m_history.push_back(SetLocation);
        m_zone = zone_;
        m_name = name_;
    }

    void open_appointment(const std::string& url_) {
        m_history.push_back(OpenAppointment);
        m_url = url_;
    }

    void set_calendar_date(const DateTime& date_time_) {
        m_history.push_back(SetCalendarDate);
        m_date_time = date_time_;
    }

private:
    std::string m_url;
    std::string m_zone;
    std::string m_name;
    DateTime m_date_time;
    std::vector<Action> m_history;
};

} // namespace datetime
} // namespace indicator
} // namespace unity

#endif // INDICATOR_DATETIME_ACTIONS_MOCK_H
