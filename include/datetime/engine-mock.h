/*
 * Copyright 2014 Canonical Ltd.
 * Copyright 2021 Robert Tari
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
 *   Robert Tari <robert@tari.in>
 */

#ifndef INDICATOR_DATETIME_ENGINE_MOCK_H
#define INDICATOR_DATETIME_ENGINE_MOCK_H

#include <datetime/engine.h>

namespace ayatana {
namespace indicator {
namespace datetime {

/****
*****
****/

/**
 * A no-op #Engine
 *
 * @see Engine
 */
class MockEngine: public Engine
{
public:
    MockEngine() =default;
    ~MockEngine() =default;

    void get_appointments(const DateTime& /*begin*/,
                          const DateTime& /*end*/,
                          const Timezone& /*default_timezone*/,
                          std::function<void(const std::vector<Appointment>&)> appointment_func) override {
        appointment_func(m_appointments);
    }

    core::Signal<>& changed() override {
        return m_changed;
    }

    void disable_alarm(const Appointment&) override {
    }

private:
    core::Signal<> m_changed;
    std::vector<Appointment> m_appointments;
};

/***
****
***/

} // namespace datetime
} // namespace indicator
} // namespace ayatana

#endif // INDICATOR_DATETIME_ENGINE_MOCK_H
