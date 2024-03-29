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

#ifndef INDICATOR_DATETIME_SNAP_H
#define INDICATOR_DATETIME_SNAP_H

#include <datetime/appointment.h>
#include <datetime/settings.h>

#include <notifications/notifications.h>
#include <notifications/sound.h>

#include <gio/gio.h> // GDBusConnection

#include <functional>
#include <memory>

namespace ayatana {
namespace indicator {
namespace datetime {

/**
 * \brief Pops up Snap Decisions for appointments
 */
class Snap
{
public:
    Snap(const std::shared_ptr<ayatana::indicator::notifications::Engine>& engine,
         const std::shared_ptr<ayatana::indicator::notifications::SoundBuilder>& sound_builder,
         const std::shared_ptr<const Settings>& settings,
         GDBusConnection* system_bus);
    virtual ~Snap();

    enum class Response { None, Snooze, ShowApp };
    typedef std::function<void(const Appointment&, const Alarm&, const Response&)> response_func;
    void operator()(const Appointment& appointment,
                    const Alarm& alarm,
                    response_func on_response);

private:
    class Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace datetime
} // namespace indicator
} // namespace ayatana

#endif // INDICATOR_DATETIME_SNAP_H
