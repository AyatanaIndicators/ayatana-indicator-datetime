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

#ifndef INDICATOR_DATETIME_STATE_LIVE_H
#define INDICATOR_DATETIME_STATE_LIVE_H

#include <datetime/state.h>

namespace unity {
namespace indicator {
namespace datetime {

/***
****
***/

class LiveState: public State
{
public:
    LiveState();
    virtual ~LiveState() =default;
};

/***
****
***/


} // namespace datetime
} // namespace indicator
} // namespace unity

#endif // INDICATOR_DATETIME_SETTINGS_LIVE_H
