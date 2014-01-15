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
#include <datetime/settings-shared.h>

#include <glib.h>
#include <gio/gio.h>

namespace unity {
namespace indicator {
namespace datetime {

/***
****
***/

class DesktopFormatter::Impl
{
public:

Impl(DesktopFormatter * owner, const std::shared_ptr<Clock>& clock):
    m_owner(owner),
    m_clock(clock),
    m_settings(g_settings_new(SETTINGS_INTERFACE))
{
    const gchar * const keys[] = { "changed::" SETTINGS_SHOW_SECONDS_S,
                                   "changed::" SETTINGS_TIME_FORMAT_S,
                                   "changed::" SETTINGS_TIME_FORMAT_S,
                                   "changed::" SETTINGS_CUSTOM_TIME_FORMAT_S,
                                   "changed::" SETTINGS_SHOW_DAY_S,
                                   "changed::" SETTINGS_SHOW_DATE_S,
                                   "changed::" SETTINGS_SHOW_YEAR_S };
    for(const auto& key : keys)
        g_signal_connect(m_settings, key, G_CALLBACK(onSettingsChanged), this);

    rebuildHeaderFormat();
}

~Impl()
{
    g_signal_handlers_disconnect_by_data(m_settings, this);
    g_object_unref(m_settings);
}

private:

static void onSettingsChanged(GSettings   * /*changed*/,
                              const gchar * /*key*/,
                              gpointer      gself)
{
    static_cast<Impl*>(gself)->rebuildHeaderFormat();
}

void rebuildHeaderFormat()
{
    auto fmt = getHeaderLabelFormatString(m_settings);
    m_owner->headerFormat.set(fmt);
    g_free(fmt);
}

private:

gchar* getHeaderLabelFormatString(GSettings* s) const
{
    char * fmt;
    const auto mode = g_settings_get_enum(s, SETTINGS_TIME_FORMAT_S);

    if (mode == TIME_FORMAT_MODE_CUSTOM)
    {
        fmt = g_settings_get_string(s, SETTINGS_CUSTOM_TIME_FORMAT_S);
    }
    else
    {
        const auto show_day = g_settings_get_boolean(s, SETTINGS_SHOW_DAY_S);
        const auto show_date = g_settings_get_boolean(s, SETTINGS_SHOW_DATE_S);
        const auto show_year = show_date && g_settings_get_boolean(s, SETTINGS_SHOW_YEAR_S);
        const auto date_fmt = getDateFormat(show_day, show_date, show_year);
        const auto time_fmt = getFullTimeFormatString(s);
        fmt = joinDateAndTimeFormatStrings(date_fmt, time_fmt);
    }

    return fmt;
}

const gchar* T_(const gchar* in) const
{
    return m_owner->T_(in);
}

const gchar* getDateFormat(bool show_day, bool show_date, bool show_year) const
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

const gchar* getFullTimeFormatString(GSettings* settings) const
{
    auto show_seconds = g_settings_get_boolean(settings, SETTINGS_SHOW_SECONDS_S);

    bool twelvehour;
    switch (g_settings_get_enum(settings, SETTINGS_TIME_FORMAT_S))
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

    return m_owner->getDefaultHeaderTimeFormat(twelvehour, show_seconds);
}

gchar* joinDateAndTimeFormatStrings(const char* date_string,
                                    const char* time_string) const
{
    gchar * str;

    if (date_string && time_string)
    {
        /* TRANSLATORS: This is a format string passed to strftime to
         * combine the date and the time.  The value of "%s\u2003%s"
         * will result in a string like this in US English 12-hour time:
         * 'Fri Jul 16 11:50 AM'. The space in between date and time is
         * a Unicode en space (E28082 in UTF-8 hex). */
        str =  g_strdup_printf("%s\u2003%s", date_string, time_string);
    }
    else if (date_string)
    {
        str = g_strdup(date_string);
    }
    else // time_string
    {
        str = g_strdup(time_string);
    }

    return str;
}

private:

DesktopFormatter * const m_owner;
std::shared_ptr<Clock> m_clock;
GSettings * m_settings;
};

/***
****
***/

DesktopFormatter::DesktopFormatter(const std::shared_ptr<Clock>& clock):
    Formatter(clock),
    p(new Impl(this, clock))
{
}

DesktopFormatter::~DesktopFormatter() = default;

/***
****
***/

} // namespace datetime
} // namespace indicator
} // namespace unity
