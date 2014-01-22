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

#include <datetime/date-time.h>

namespace unity {
namespace indicator {
namespace datetime {

/***
****
***/

DateTime::DateTime(time_t t)
{
    GDateTime * gdt = g_date_time_new_from_unix_local(t);
    reset(gdt);
    g_date_time_unref(gdt);
}

DateTime DateTime::NowLocal()
{
    GDateTime * gdt = g_date_time_new_now_local();
    DateTime dt(gdt);
    g_date_time_unref(gdt);
    return dt;
}

DateTime DateTime::to_timezone(const std::string& zone) const
{
    auto gtz = g_time_zone_new(zone.c_str());
    auto gdt = g_date_time_to_timezone(get(), gtz);
    DateTime dt(gdt);
    g_time_zone_unref(gtz);
    g_date_time_unref(gdt);
    return dt;
}

GDateTime* DateTime::get() const
{
    g_assert(m_dt);
    return m_dt.get();
}

std::string DateTime::format(const std::string& fmt) const
{
    const auto str = g_date_time_format(get(), fmt.c_str());
    std::string ret = str;
    g_free(str);
    return ret;
}

int DateTime::day_of_month() const
{
    return g_date_time_get_day_of_month(get());
}

int64_t DateTime::to_unix() const
{
    return g_date_time_to_unix(get());
}

void DateTime::reset(GDateTime* in)
{
    if (in)
    {
        auto deleter = [](GDateTime* dt){g_date_time_unref(dt);};
        m_dt = std::shared_ptr<GDateTime>(g_date_time_ref(in), deleter);
        g_assert(m_dt);
    }
    else
    {
        m_dt.reset();
    }
}

bool DateTime::operator<(const DateTime& that) const
{
    return g_date_time_compare(get(), that.get()) < 0;
}

bool DateTime::operator!=(const DateTime& that) const
{
    // return true if this isn't set, or if it's not equal
    return (!m_dt) || !(*this == that);
}

bool DateTime::operator==(const DateTime& that) const
{
    auto dt = get();
    auto tdt = that.get();
    if (!dt && !tdt) return true;
    if (!dt || !tdt) return false;
    return g_date_time_compare(get(), that.get()) == 0;
}

bool DateTime::is_same_day(const DateTime& a, const DateTime& b)
{
    // it's meaningless to compare uninitialized dates
    if (!a.m_dt || !b.m_dt)
        return false;

    const auto adt = a.get();
    const auto bdt = b.get();
    return (g_date_time_get_year(adt) == g_date_time_get_year(bdt))
        && (g_date_time_get_day_of_year(adt) == g_date_time_get_day_of_year(bdt));
}

bool DateTime::is_same_minute(const DateTime& a, const DateTime& b)
{
    if (!is_same_day(a,b))
        return false;

    const auto adt = a.get();
    const auto bdt = b.get();
    return (g_date_time_get_hour(adt) == g_date_time_get_hour(bdt))
        && (g_date_time_get_minute(adt) == g_date_time_get_minute(bdt));
}

/***
****
***/

} // namespace datetime
} // namespace indicator
} // namespace unity
