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

#include <datetime/date-time.h>
#include <datetime/timezones.h>

#include <core/property.h>
#include <core/signal.h>

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
    virtual DateTime localtime() const =0;
    core::Property<std::set<std::string>> timezones;
    core::Signal<> skewDetected;
    core::Signal<> dateChanged;

protected:
    Clock();

private:
    static void onSystemBusReady(GObject*, GAsyncResult*, gpointer);
    static void onPrepareForSleep(GDBusConnection*, const gchar*, const gchar*, const gchar*, const gchar*, GVariant*, gpointer);

    GCancellable * m_cancellable = nullptr;
    GDBusConnection * m_system_bus = nullptr;
    unsigned int m_sleep_subscription_id = 0;

    // we've got raw pointers and GSignal tags in here, so disable copying
    Clock(const Clock&) =delete;
    Clock& operator=(const Clock&) =delete;
};

/***
****
***/

/**
 * \brief A live clock that provides the actual system time.
 *
 * This subclass also adds another clock skew detection test:
 * it wakes up every skewTestIntervalSec seconds to see how
 * much time has passed since the last wakeup. If the answer
 * isn't what it expected, the skewDetected signal is triggered.
 */
class LiveClock: public Clock
{
public:
    LiveClock (const std::shared_ptr<Timezones>& zones);
    virtual ~LiveClock();
    virtual DateTime localtime() const;
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
