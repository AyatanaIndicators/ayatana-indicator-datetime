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

#include <memory> // std::shared_ptr

namespace unity {
namespace indicator {
namespace datetime {

/**
 * C++ wrapper class for GDateTime
 */
class DateTime
{
public:

    GDateTime* get() const
    {
        return dt_.get();
    }

    GDateTime* operator()() const
    {
        return get();
    }

    void set (GDateTime* in) {
        auto deleter = [](GDateTime* dt){g_date_time_unref(dt);};
        dt_ = std::shared_ptr<GDateTime>(g_date_time_ref(in), deleter);
    }

    DateTime& operator=(GDateTime* in)
    {
        set (in);
        return *this;
    }

    DateTime& operator=(const DateTime& in)
    {
        set (in.get());
        return *this;
    }

    bool operator<(const DateTime& that) const
    {
        return g_date_time_compare (get(), that.get()) < 0;
    }

    bool operator!=(const DateTime& that) const
    {
        return !(*this == that);
    }

    bool operator==(const DateTime& that) const
    {
         GDateTime * dt = get();
         GDateTime * tdt = that.get();
         if (!dt && !tdt) return true;
         if (!dt || !tdt) return false;
        return g_date_time_compare (get(), that.get()) == 0;
    }

private:

    std::shared_ptr<GDateTime> dt_;
};

} // namespace datetime
} // namespace indicator
} // namespace unity

#endif // INDICATOR_DATETIME_DATETIME_H
