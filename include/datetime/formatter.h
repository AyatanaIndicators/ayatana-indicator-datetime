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

#ifndef INDICATOR_DATETIME_FORMATTER_H
#define INDICATOR_DATETIME_FORMATTER_H

#include <core/property.h>
#include <core/signal.h>

#include <glib.h>

namespace unity {
namespace indicator {
namespace datetime {

class Clock;
class DateTime;

/***
****
***/

/**
 * \brief Provides the right time format strings based on the profile and user's settings
 */
class Formatter
{
public:

    /** \brief The time format string for the menu header */
    core::Property<std::string> headerFormat;

    /** \brief The time string for the menu header. (eg, the headerFormat + the clock's time */
    core::Property<std::string> header;

    /** \brief Signal to denote when the relativeFormat has changed.
               When this is emitted, clients will want to rebuild their
               menuitems that contain relative time strings */
    core::Signal<> relativeFormatChanged;

    /** \brief Generate a relative time format for some time (or time range)
               from the current clock's value. For example, a full-day interval
               starting at the end of the current clock's day yields "Tomorrow" */
    std::string getRelativeFormat(GDateTime* then, GDateTime* then_end=nullptr) const;

protected:

    Formatter(const std::shared_ptr<Clock>&);
    virtual ~Formatter();

    /** \brief Returns true if the current locale prefers 12h display instead of 24h */
    static bool is_locale_12h();

    static const char * getDefaultHeaderTimeFormat(bool twelvehour, bool show_seconds);

    /** \brief Translate the string based on LC_TIME instead of LC_MESSAGES.
               The intent of this is to let users set LC_TIME to override
               their other locale settings when generating time format string */
    static const char * T_(const char * fmt);

private:

    Formatter(const Formatter&) =delete;
    Formatter& operator=(const Formatter&) =delete;

    class Impl;
    std::unique_ptr<Impl> p;
};


/**
 * \brief A Formatter for the Desktop and DesktopGreeter profiles.
 */
class DesktopFormatter: public Formatter
{
public:
    DesktopFormatter(const std::shared_ptr<Clock>&);
    ~DesktopFormatter();

private:
    class Impl;
    friend Impl;
    std::unique_ptr<Impl> p;
};


/**
 * \brief A Formatter for Phone and PhoneGreeter profiles.
 */
class PhoneFormatter: public Formatter
{
public:
    PhoneFormatter(const std::shared_ptr<Clock>& clock): Formatter(clock) {
        headerFormat.set(getDefaultHeaderTimeFormat(is_locale_12h(), false));
    }
};

} // namespace datetime
} // namespace indicator
} // namespace unity

#endif // INDICATOR_DATETIME_CLOCK_H
