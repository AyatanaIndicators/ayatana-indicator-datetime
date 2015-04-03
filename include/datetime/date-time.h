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

#ifndef INDICATOR_DATETIME_DATETIME_H
#define INDICATOR_DATETIME_DATETIME_H

#include <glib.h> // GDateTime

#include <chrono>
#include <ctime> // time_t
#include <memory> // std::shared_ptr

namespace unity {
namespace indicator {
namespace datetime {

/**
 * \brief A simple C++ wrapper for GDateTime to simplify ownership/refcounts
 */
class DateTime
{
public:
    static DateTime NowLocal();
    static DateTime Local(int year, int month, int day, int hour, int minute, double seconds);

    DateTime();
    explicit DateTime(time_t t);
    DateTime(GTimeZone* tz, GDateTime* dt);
    DateTime(GTimeZone* tz, int year, int month, int day, int hour, int minute, double seconds);
    DateTime& operator=(const DateTime& in);
    DateTime& operator+=(const std::chrono::minutes&);
    DateTime& operator+=(const std::chrono::seconds&);
    DateTime to_timezone(const std::string& zone) const;
    DateTime start_of_month() const;
    DateTime start_of_day() const;
    DateTime start_of_minute() const;
    DateTime end_of_day() const;
    DateTime end_of_month() const;
    DateTime add_days(int days) const;
    DateTime add_full(int year, int month, int day, int hour, int minute, double seconds) const;

    GDateTime* get() const;
    GDateTime* operator()() const {return get();}

    std::string format(const std::string& fmt) const;
    void ymd(int& year, int& month, int& day) const;
    int day_of_month() const;
    int hour() const;
    int minute() const;
    double seconds() const;
    int64_t to_unix() const;

    bool operator<(const DateTime& that) const;
    bool operator<=(const DateTime& that) const;
    bool operator!=(const DateTime& that) const;
    bool operator==(const DateTime& that) const;
    int64_t operator- (const DateTime& that) const;

    static bool is_same_day(const DateTime& a, const DateTime& b);
    static bool is_same_minute(const DateTime& a, const DateTime& b);

    bool is_set() const { return m_tz && m_dt; }

private:
    void reset(GTimeZone*, GDateTime*);
    std::shared_ptr<GTimeZone> m_tz;
    std::shared_ptr<GDateTime> m_dt;
};

} // namespace datetime
} // namespace indicator
} // namespace unity

#endif // INDICATOR_DATETIME_DATETIME_H
