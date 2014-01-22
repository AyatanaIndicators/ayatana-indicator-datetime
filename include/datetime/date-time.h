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
    explicit DateTime(time_t t);
    explicit DateTime(GDateTime* in=nullptr) {reset(in);}
    DateTime& operator=(GDateTime* in) {reset(in); return *this;}
    DateTime& operator=(const DateTime& in) {m_dt=in.m_dt; return *this; }
    DateTime to_timezone(const std::string& zone) const;
    void reset(GDateTime* in=nullptr);

    GDateTime* get() const;
    GDateTime* operator()() const {return get();}

    std::string format(const std::string& fmt) const;
    int day_of_month() const;
    int64_t to_unix() const;
    int day_of_year() const;
    gint64 difference(const DateTime& that) const;

    bool operator<(const DateTime& that) const;
    bool operator!=(const DateTime& that) const;
    bool operator==(const DateTime& that) const;

private:
    std::shared_ptr<GDateTime> m_dt;
};

} // namespace datetime
} // namespace indicator
} // namespace unity

#endif // INDICATOR_DATETIME_DATETIME_H
