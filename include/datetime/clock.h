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

#ifndef INDICATOR_DATETIME_CLOCK_H
#define INDICATOR_DATETIME_CLOCK_H

#include <datetime/timezones.h>

#include <core/property.h>
#include <core/signal.h>

#include <glib.h>
#include <gio/gio.h>

#include <set>
#include <string>

namespace unity {
namespace indicator {
namespace datetime {

/**
 * \brief A clock.
 *
 * Provides a signal to notify when clock skew is detected, such as
 * when the timezone changes or when the system resumes from sleep.
 */
class Clock
{
public:
    virtual ~Clock();
    virtual GDateTime* localtime() const = 0;
    core::Property<std::set<std::string> > timezones;
    core::Signal<> skewDetected;

protected:
    Clock();

private:
    static void onSystemBusReady(GObject*, GAsyncResult*, gpointer);
    static void onPrepareForSleep(GDBusConnection*, const gchar*, const gchar*, const gchar*, const gchar*, GVariant*, gpointer);

    Clock(const Clock&) =delete;
    Clock& operator=(const Clock&) =delete;

    GCancellable * cancellable_ = nullptr;
    GDBusConnection * system_bus_ = nullptr;
    unsigned int sleep_subscription_id_ = 0;
};

/***
****
***/

/**
 * \brief A live clock that provides the actual system time.
 *
 * Adds another clock skew detection test: wakes up every
 * skewTestIntervalSec seconds to see how much time has passed
 * since the last time it checked.
 */
class LiveClock: public Clock
{
public:
    LiveClock (const std::shared_ptr<Timezones>& zones);
    virtual ~LiveClock();
    virtual GDateTime* localtime() const;
    core::Property<unsigned int> skewTestIntervalSec;

private:
    class Impl;
    std::unique_ptr<Impl> p;
};

/***
****
***/

} // namespace datetime
} // namespace indicator
} // namespace unity

#endif // INDICATOR_DATETIME_CLOCK_H
