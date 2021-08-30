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

namespace ayatana {
namespace indicator {
namespace datetime {

class MockActions: public Actions
{
public:
    explicit MockActions(const std::shared_ptr<State>& state_in): Actions(state_in) {}
    ~MockActions() =default;

    enum Action { OpenAlarmApp,
                  OpenAppt,
                  OpenCalendarApp,
                  OpenSettingsApp,
                  SetLocation };

    const std::vector<Action>& history() const { return m_history; }
    const DateTime& date_time() const { return m_date_time; }
    const std::string& zone() const { return m_zone; }
    const std::string& name() const { return m_name; }
    const Appointment& appointment() const { return m_appt; }
    void clear() { m_history.clear(); m_zone.clear(); m_name.clear(); }

    bool desktop_has_calendar_app() const {
        return m_desktop_has_calendar_app;
    }
    std::string open_alarm_app() {
        m_history.push_back(OpenAlarmApp);

        return "";
    }
    std::string open_appointment(const Appointment& appt, const DateTime& dt) {
        m_appt = appt;
        m_date_time = dt;
        m_history.push_back(OpenAppt);

        return "";
    }
    std::string open_calendar_app(const DateTime& dt) {
        m_date_time = dt;
        m_history.push_back(OpenCalendarApp);

        return "";
    }
    std::string open_settings_app() {
        m_history.push_back(OpenSettingsApp);

        return "";
    }

    void set_location(const std::string& zone_, const std::string& name_) {
        m_history.push_back(SetLocation);
        m_zone = zone_;
        m_name = name_;
    }

    void set_desktop_has_calendar_app(bool b) {
        m_desktop_has_calendar_app = b;
    }

private:
    bool m_desktop_has_calendar_app = true;
    Appointment m_appt;
    std::string m_zone;
    std::string m_name;
    DateTime m_date_time;
    std::vector<Action> m_history;
};

} // namespace datetime
} // namespace indicator
} // namespace ayatana

#endif // INDICATOR_DATETIME_ACTIONS_MOCK_H
