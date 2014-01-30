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

#include <datetime/formatter.h>
#include <datetime/utils.h> // T_()

namespace unity {
namespace indicator {
namespace datetime {

/***
****
***/

namespace
{

std::string joinDateAndTimeFormatStrings(const char* date_string,
                                         const char* time_string)
{
    std::string str;

    if (date_string && time_string)
    {
        /* TRANSLATORS: This is a format string passed to strftime to
         * combine the date and the time.  The value of "%s\u2003%s"
         * will result in a string like this in US English 12-hour time:
         * 'Fri Jul 16 11:50 AM'. The space in between date and time is
         * a Unicode en space (E28082 in UTF-8 hex). */
        str = date_string;
        str += "\u2003";
        str += time_string;
    }
    else if (date_string)
    {
        str = date_string;
    }
    else // time_string
    {
        str = time_string;
    }

    return str;
}
} // unnamed namespace

/***
****
***/

DesktopFormatter::DesktopFormatter(const std::shared_ptr<const Clock>&    clock_in,
                                   const std::shared_ptr<const Settings>& settings_in):
    Formatter(clock_in),
    m_settings(settings_in)
{
    m_settings->show_day.changed().connect([this](bool){rebuildHeaderFormat();});
    m_settings->show_date.changed().connect([this](bool){rebuildHeaderFormat();});
    m_settings->show_year.changed().connect([this](bool){rebuildHeaderFormat();});
    m_settings->show_seconds.changed().connect([this](bool){rebuildHeaderFormat();});
    m_settings->time_format_mode.changed().connect([this](TimeFormatMode){rebuildHeaderFormat();});
    m_settings->custom_time_format.changed().connect([this](const std::string&){rebuildHeaderFormat();});

    rebuildHeaderFormat();
}

void DesktopFormatter::rebuildHeaderFormat()
{
    headerFormat.set(getHeaderLabelFormatString());
}

std::string DesktopFormatter::getHeaderLabelFormatString() const
{
    std::string fmt;
    const auto mode = m_settings->time_format_mode.get();

    if (mode == TIME_FORMAT_MODE_CUSTOM)
    {
        fmt = m_settings->custom_time_format.get();
    }
    else
    {
        const auto show_day = m_settings->show_day.get();
        const auto show_date = m_settings->show_date.get();
        const auto show_year = show_date && m_settings->show_year.get();
        const auto date_fmt = getDateFormat(show_day, show_date, show_year);
        const auto time_fmt = getFullTimeFormatString();
        fmt = joinDateAndTimeFormatStrings(date_fmt, time_fmt);
    }

    return fmt;
}

const gchar* DesktopFormatter::getFullTimeFormatString() const
{
    const auto show_seconds = m_settings->show_seconds.get();

    bool twelvehour;
    switch (m_settings->time_format_mode.get())
    {
    case TIME_FORMAT_MODE_LOCALE_DEFAULT:
        twelvehour = is_locale_12h();
        break;

    case TIME_FORMAT_MODE_24_HOUR:
        twelvehour = false;
        break;

    default:
        twelvehour = true;
        break;
    }

    return getDefaultHeaderTimeFormat(twelvehour, show_seconds);
}

const gchar* DesktopFormatter::getDateFormat(bool show_day, bool show_date, bool show_year) const
{
    const char * fmt;

    if (show_day && show_date && show_year)
        /* TRANSLATORS: a strftime(3) format showing the weekday, date, and year */
        fmt = T_("%a %b %e %Y");
    else if (show_day && show_date)
        /* TRANSLATORS: a strftime(3) format showing the weekday and date */
        fmt = T_("%a %b %e");
    else if (show_day && show_year)
        /* TRANSLATORS: a strftime(3) format showing the weekday and year. */
        fmt = T_("%a %Y");
    else if (show_day)
        /* TRANSLATORS: a strftime(3) format showing the weekday. */
        fmt = T_("%a");
    else if (show_date && show_year)
        /* TRANSLATORS: a strftime(3) format showing the date and year */
        fmt = T_("%b %e %Y");
    else if (show_date)
        /* TRANSLATORS: a strftime(3) format showing the date */
        fmt = T_("%b %e");
    else if (show_year)
        /* TRANSLATORS: a strftime(3) format showing the year */
        fmt = T_("%Y");
    else
        fmt = nullptr;

    return fmt;
}

/***
****
***/

} // namespace datetime
} // namespace indicator
} // namespace unity
