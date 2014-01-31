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

#ifndef INDICATOR_DATETIME_ACTIONS_LIVE_H
#define INDICATOR_DATETIME_ACTIONS_LIVE_H

#include <datetime/actions.h>

namespace unity {
namespace indicator {
namespace datetime {

/**
 * \brief Production implementation of the Actions interface.
 *
 * Delegates URLs, sets the timezone via org.freedesktop.timedate1, etc.
 *
 * @see MockActions
 */
class LiveActions: public Actions
{
public:
    LiveActions(const std::shared_ptr<State>& state_in);
    ~LiveActions() =default;

    void open_desktop_settings();
    void open_phone_settings();
    void open_phone_clock_app();
    void open_planner();
    void open_planner_at(const DateTime&);
    void open_appointment(const std::string& uid);
    void set_location(const std::string& zone, const std::string& name);
    void set_calendar_date(const DateTime&);

protected:
    virtual void execute_command(const std::string& command);
    virtual void dispatch_url(const std::string& url);
};

} // namespace datetime
} // namespace indicator
} // namespace unity

#endif // INDICATOR_DATETIME_ACTIONS_H
