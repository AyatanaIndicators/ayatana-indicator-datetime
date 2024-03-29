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

#ifndef INDICATOR_DATETIME_ACTIONS_H
#define INDICATOR_DATETIME_ACTIONS_H

#include <datetime/date-time.h>
#include <datetime/state.h>

#include <memory> // shared_ptr
#include <string>

#include <gio/gio.h> // GSimpleActionGroup

namespace ayatana {
namespace indicator {
namespace datetime {

/**
 * \brief Interface for all the actions that can be activated by users.
 *
 * This is a simple C++ wrapper around our GActionGroup that gets exported
 * onto the bus. Subclasses implement the actual code that should be run
 * when a particular action is triggered.
 */
class Actions
{
public:

    virtual bool desktop_has_calendar_app() const =0;

    virtual std::string open_alarm_app() =0;
    virtual std::string open_appointment(const Appointment&, const DateTime&) =0;
    virtual std::string open_calendar_app(const DateTime&) =0;
    virtual std::string open_settings_app() =0;

    virtual void set_location(const std::string& zone, const std::string& name)=0;

    void set_calendar_date(const DateTime&, bool bUpdateCalendar);
    GActionGroup* action_group();
    const std::shared_ptr<State> state() const;

protected:
    explicit Actions(const std::shared_ptr<State>& state);
    virtual ~Actions();

private:
    std::shared_ptr<State> m_state;
    GSimpleActionGroup* m_actions = nullptr;
    void update_calendar_state();

    // we've got raw pointers in here, so disable copying
    Actions(const Actions&) =delete;
    Actions& operator=(const Actions&) =delete;
};

} // namespace datetime
} // namespace indicator
} // namespace ayatana

#endif // INDICATOR_DATETIME_ACTIONS_H
