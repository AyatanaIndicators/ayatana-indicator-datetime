/*
 * Copyright 2013 Canonical Ltd.
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
 *   Ted Gould <ted@canonical.com>
 *   Charles Kerr <charles.kerr@canonical.com>
 *   Robert Tari <robert@tari.in>
 */

#ifndef AYATANA_INDICATOR_NOTIFICATIONS_DBUS_SHARED_H
#define AYATANA_INDICATOR_NOTIFICATIONS_DBUS_SHARED_H

#define BUS_SCREEN_NAME      "com.canonical.Unity.Screen"
#define BUS_SCREEN_PATH      "/com/canonical/Unity/Screen"
#define BUS_SCREEN_INTERFACE "com.canonical.Unity.Screen"

#define BUS_POWERD_NAME      "com.lomiri.Repowerd"
#define BUS_POWERD_PATH      "/com/lomiri/Repowerd"
#define BUS_POWERD_INTERFACE "com.lomiri.Repowerd"

//TODO: Reimplement using hfd-service
//#define BUS_HAPTIC_NAME      ""
//#define BUS_HAPTIC_PATH      ""
//#define BUS_HAPTIC_INTERFACE ""

#endif /* INDICATOR_NOTIFICATIONS_DBUS_SHARED_H */
