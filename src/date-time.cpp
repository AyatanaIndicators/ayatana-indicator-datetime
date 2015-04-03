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

DateTime::DateTime()
{
}

DateTime::DateTime(GTimeZone* gtz, GDateTime* gdt)
{
    g_return_if_fail(gtz!=nullptr);
    g_return_if_fail(gdt!=nullptr);

    reset(gtz, gdt);
}

DateTime::DateTime(GTimeZone* gtz, int year, int month, int day, int hour, int minute, double seconds)
{
    g_return_if_fail(gtz!=nullptr);

    auto gdt = g_date_time_new(gtz, year, month, day, hour, minute, seconds);
    reset(gtz, gdt);
    g_date_time_unref(gdt);
}

DateTime& DateTime::operator=(const DateTime& that)
{
    m_tz = that.m_tz;
    m_dt = that.m_dt;
    return *this;
}

DateTime& DateTime::operator+=(const std::chrono::minutes& minutes)
{
    return (*this = add_full(0, 0, 0, 0, minutes.count(), 0));
}

DateTime& DateTime::operator+=(const std::chrono::seconds& seconds)
{
    return (*this = add_full(0, 0, 0, 0, 0, seconds.count()));
}

DateTime::DateTime(time_t t)
{
    auto gtz = g_time_zone_new_local();
    auto gdt = g_date_time_new_from_unix_local(t);
    reset(gtz, gdt);
    g_time_zone_unref(gtz);
    g_date_time_unref(gdt);
}

DateTime DateTime::NowLocal()
{
    auto gtz = g_time_zone_new_local();
    auto gdt = g_date_time_new_now(gtz);
    DateTime dt(gtz, gdt);
    g_time_zone_unref(gtz);
    g_date_time_unref(gdt);
    return dt;
}

DateTime DateTime::Local(int year, int month, int day, int hour, int minute, double seconds)
{
    auto gtz = g_time_zone_new_local();
    DateTime dt(gtz, year, month, day, hour, minute, seconds);
    g_time_zone_unref(gtz);
    return dt;
}

DateTime DateTime::to_timezone(const std::string& zone) const
{
    auto gtz = g_time_zone_new(zone.c_str());
    auto gdt = g_date_time_to_timezone(get(), gtz);
    DateTime dt(gtz, gdt);
    g_time_zone_unref(gtz);
    g_date_time_unref(gdt);
    return dt;
}

DateTime DateTime::end_of_day() const
{
    g_assert(is_set());

    return add_days(1).start_of_day().add_full(0,0,0,0,0,-1);
}

DateTime DateTime::end_of_month() const
{
    g_assert(is_set());

    return add_full(0,1,0,0,0,0).start_of_month().add_full(0,0,0,0,0,-1);
}

DateTime DateTime::start_of_month() const
{
    g_assert(is_set());

    int year=0, month=0, day=0;
    ymd(year, month, day);
    return DateTime(m_tz.get(), year, month, 1, 0, 0, 0);
}

DateTime DateTime::start_of_day() const
{
    g_assert(is_set());

    int year=0, month=0, day=0;
    ymd(year, month, day);
    return DateTime(m_tz.get(), year, month, day, 0, 0, 0);
}

DateTime DateTime::start_of_minute() const
{
    g_assert(is_set());

    int year=0, month=0, day=0;
    ymd(year, month, day);
    return DateTime(m_tz.get(), year, month, day, hour(), minute(), 0);
}

DateTime DateTime::add_full(int year, int month, int day, int hour, int minute, double seconds) const
{
    auto gdt = g_date_time_add_full(get(), year, month, day, hour, minute, seconds);
    DateTime dt(m_tz.get(), gdt);
    g_date_time_unref(gdt);
    return dt;
}

DateTime DateTime::add_days(int days) const
{
    return add_full(0, 0, days, 0, 0, 0);
}

GDateTime* DateTime::get() const
{
    g_assert(m_dt);
    return m_dt.get();
}

std::string DateTime::format(const std::string& fmt) const
{
    std::string ret;

    gchar* str = g_date_time_format(get(), fmt.c_str());
    if (str)
    {
        ret = str;
        g_free(str);
    }

    return ret;
}

void DateTime::ymd(int& year, int& month, int& day) const
{
    g_date_time_get_ymd(get(), &year, &month, &day);
}

int DateTime::day_of_month() const
{
    return g_date_time_get_day_of_month(get());
}

int DateTime::hour() const
{
    return g_date_time_get_hour(get());
}

int DateTime::minute() const
{
    return g_date_time_get_minute(get());
}

double DateTime::seconds() const
{
    return g_date_time_get_seconds(get());
}

int64_t DateTime::to_unix() const
{
    return g_date_time_to_unix(get());
}

void DateTime::reset(GTimeZone* gtz, GDateTime* gdt)
{
    g_return_if_fail (gdt!=nullptr);
    g_return_if_fail (gtz!=nullptr);

    auto tz_deleter = [](GTimeZone* tz){g_time_zone_unref(tz);};
    m_tz = std::shared_ptr<GTimeZone>(g_time_zone_ref(gtz), tz_deleter);

    auto dt_deleter = [](GDateTime* dt){g_date_time_unref(dt);};
    m_dt = std::shared_ptr<GDateTime>(g_date_time_ref(gdt), dt_deleter);
}

bool DateTime::operator<(const DateTime& that) const
{
    return g_date_time_compare(get(), that.get()) < 0;
}

bool DateTime::operator<=(const DateTime& that) const
{
    return g_date_time_compare(get(), that.get()) <= 0;
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

int64_t DateTime::operator- (const DateTime& that) const
{
    return g_date_time_difference(get(), that.get());
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
