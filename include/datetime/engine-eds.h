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

#ifndef INDICATOR_DATETIME_ENGINE_EDS_H
#define INDICATOR_DATETIME_ENGINE_EDS_H

#include <datetime/engine.h>

#include <datetime/appointment.h>
#include <datetime/date-time.h>
#include <datetime/timezone.h>

#include <functional>
#include <vector>

namespace ayatana {
namespace indicator {
namespace datetime {

/****
*****
****/

class Myself;

/**
 * Class wrapper around EDS so multiple #EdsPlanners can share resources
 *
 * @see EdsPlanner
 */
class EdsEngine: public Engine
{
public:
    EdsEngine();
    explicit EdsEngine(const std::unique_ptr<Myself> &myself);
    ~EdsEngine();

    void get_appointments(const DateTime& begin,
                          const DateTime& end,
                          const Timezone& default_timezone,
                          std::function<void(const std::vector<Appointment>&)> appointment_func) override;
    void disable_ubuntu_alarm(const Appointment&) override;

    core::Signal<>& changed() override;

private:
    class Impl;
    std::unique_ptr<Impl> p;

    // we've got a unique_ptr here, disable copying...
    EdsEngine(const EdsEngine&) =delete;
    EdsEngine& operator=(const EdsEngine&) =delete;
};

/***
****
***/

} // namespace datetime
} // namespace indicator
} // namespace ayatana

#endif // INDICATOR_DATETIME_ENGINE_EDS_H
