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

#ifndef UNITY_INDICATOR_NOTIFICATIONS_AWAKE_H
#define UNITY_INDICATOR_NOTIFICATIONS_AWAKE_H

#include <memory>

namespace unity {
namespace indicator {
namespace notifications {

/***
****
***/

/**
 * A class that forces the screen display on and inhibits sleep
 */
class Awake
{
public:
    explicit Awake(const std::string& app_name);
    ~Awake();

private:
    class Impl;
    std::unique_ptr<Impl> impl;
};

/***
****
***/

} // namespace notifications
} // namespace indicator
} // namespace unity

#endif // UNITY_INDICATOR_NOTIFICATIONS_AWAKE_H
