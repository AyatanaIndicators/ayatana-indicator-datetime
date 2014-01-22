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

    explicit DateTime(GDateTime* in=nullptr) { reset(in); }

    explicit DateTime(time_t t) {
        GDateTime * gdt = g_date_time_new_from_unix_local(t);
        reset(gdt);
        g_date_time_unref(gdt);
    }

    static DateTime NowLocal() {
        GDateTime * gdt = g_date_time_new_now_local();
        DateTime dt(gdt);
        g_date_time_unref(gdt);
        return dt;
    }

    DateTime to_timezone(const std::string& zone) const {
        auto gtz = g_time_zone_new(zone.c_str());
        auto gdt = g_date_time_to_timezone(get(), gtz);
        DateTime dt(gdt);
        g_time_zone_unref(gtz);
        g_date_time_unref(gdt);
        return dt;
    }


    GDateTime* get() const {
        g_assert(m_dt);
        return m_dt.get();
    }

    GDateTime* operator()() const {
        return get();
    }


    std::string format(const std::string& fmt) const {
        auto str = g_date_time_format(get(), fmt.c_str());
        std::string ret = str;
        g_free(str);
        return ret;
    }

    int day_of_month() const { return g_date_time_get_day_of_month(get()); }

    int64_t to_unix() const { return g_date_time_to_unix(get()); }

    int day_of_year() const { return m_dt ? g_date_time_get_day_of_year(get()) : -1; }

    void reset(GDateTime* in=nullptr) {
        if (in) {
            auto deleter = [](GDateTime* dt){g_date_time_unref(dt);};
            m_dt = std::shared_ptr<GDateTime>(g_date_time_ref(in), deleter);
            g_assert(m_dt);
        } else {
            m_dt.reset();
        }
    }

    DateTime& operator=(GDateTime* in) {
        reset(in);
        return *this;
    }

    DateTime& operator=(const DateTime& in) {
        m_dt = in.m_dt;
        return *this;
    }

    gint64 difference(const DateTime& that) const {
         const auto dt = get();
         const auto tdt = that.get();

         gint64 ret;
         if (dt && tdt)
             ret = g_date_time_difference(dt, tdt);
         else if (dt)
             ret = to_unix();
         else if (tdt)
             ret = that.to_unix();
         else
             ret = 0;
         return ret;
    }

    bool operator<(const DateTime& that) const {
        return g_date_time_compare(get(), that.get()) < 0;
    }

    bool operator!=(const DateTime& that) const {
        // return true if this isn't set, or if it's not equal
        return (!m_dt) || !(*this == that);
    }

    bool operator==(const DateTime& that) const {
         GDateTime * dt = get();
         GDateTime * tdt = that.get();
         if (!dt && !tdt) return true;
         if (!dt || !tdt) return false;
        return g_date_time_compare(get(), that.get()) == 0;
    }

private:
    std::shared_ptr<GDateTime> m_dt;
};

} // namespace datetime
} // namespace indicator
} // namespace unity

#endif // INDICATOR_DATETIME_DATETIME_H
