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

#ifndef AYATANA_INDICATOR_NOTIFICATIONS_HAPTIC_H
#define AYATANA_INDICATOR_NOTIFICATIONS_HAPTIC_H

#include <memory>

namespace ayatana {
namespace indicator {
namespace notifications {

/***
****
***/

/**
 * Tries to emit haptic feedback to match the user-specified mode.
 */
class Haptic
{
public:
    enum Mode
    {
      MODE_PULSE
    };

    explicit Haptic(const Mode& mode = MODE_PULSE, bool repeat = false);
    ~Haptic();

private:
    class Impl;
    std::unique_ptr<Impl> impl;
};

/***
****
***/

} // namespace notifications
} // namespace indicator
} // namespace ayatana

#endif // AYATANA_INDICATOR_NOTIFICATIONS_HAPTIC_H
