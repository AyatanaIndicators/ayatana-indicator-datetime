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

    Impl (DesktopFormatter * owner, const std::shared_ptr<Clock>& clock):
        owner_(owner),
        clock_(clock),
        settings_(g_settings_new(SETTINGS_INTERFACE))
    {
        const gchar * const keys[] = { "changed::" SETTINGS_SHOW_SECONDS_S,
                                       "changed::" SETTINGS_TIME_FORMAT_S,
                                       "changed::" SETTINGS_TIME_FORMAT_S,
                                       "changed::" SETTINGS_CUSTOM_TIME_FORMAT_S,
                                       "changed::" SETTINGS_SHOW_DAY_S,
                                       "changed::" SETTINGS_SHOW_DATE_S,
                                       "changed::" SETTINGS_SHOW_YEAR_S };
        for (guint i=0, n=G_N_ELEMENTS(keys); i<n; i++)
            g_signal_connect(settings_, keys[i], G_CALLBACK(onSettingsChanged), this);

        rebuildHeaderFormat();
    }

    ~Impl()
    {
        g_signal_handlers_disconnect_by_data (settings_, this);
        g_object_unref (settings_);
    }

private:

    static void onSettingsChanged (GSettings   * changed G_GNUC_UNUSED,
                                   const gchar * key   G_GNUC_UNUSED,
                                   gpointer      gself)
    {
        static_cast<Impl*>(gself)->rebuildHeaderFormat();
    }

    void rebuildHeaderFormat()
    {
        gchar * fmt = getHeaderLabelFormatString (settings_);
        owner_->headerFormat.set(fmt);
        g_free (fmt);
    }

private:

    gchar* getHeaderLabelFormatString (GSettings * s) const
    {
        char * fmt;
        const TimeFormatMode mode = (TimeFormatMode) g_settings_get_enum (s, SETTINGS_TIME_FORMAT_S);

        if (mode == TIME_FORMAT_MODE_CUSTOM)
        {
            fmt = g_settings_get_string (s, SETTINGS_CUSTOM_TIME_FORMAT_S);
        }
        else
        {
            const bool show_day = g_settings_get_boolean (s, SETTINGS_SHOW_DAY_S);
            const bool show_date = g_settings_get_boolean (s, SETTINGS_SHOW_DATE_S);
            const bool show_year = show_date && g_settings_get_boolean (s, SETTINGS_SHOW_YEAR_S);
            const char * date_fmt = getDateFormat (show_day, show_date, show_year);
            const char * time_fmt = getFullTimeFormatString (s);
            fmt = joinDateAndTimeFormatStrings (date_fmt, time_fmt);
        }

        return fmt;
    }

    const gchar* T_(const gchar* in) const
    {
        return owner_->T_(in);
    }

    const gchar* getDateFormat (bool show_day, bool show_date, bool show_year) const
    {
        const char * fmt;

        if (show_day && show_date && show_year)
            /* TRANSLATORS: a strftime(3) format showing the weekday, date, and year */
            fmt = T_("%a %b %e %Y");
        else if (show_day && show_date)
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

    const gchar * getFullTimeFormatString (GSettings * settings) const
    {
        const bool show_seconds = g_settings_get_boolean (settings, SETTINGS_SHOW_SECONDS_S);

        bool twelvehour;
        switch (g_settings_get_enum (settings, SETTINGS_TIME_FORMAT_S))
        {
            case TIME_FORMAT_MODE_LOCALE_DEFAULT:
                twelvehour = is_locale_12h();
                break;

            case TIME_FORMAT_MODE_24_HOUR:
                twelvehour = FALSE;
                break;

            default:
                twelvehour = TRUE;
                break;
        }

        return owner_->getDefaultHeaderTimeFormat (twelvehour, show_seconds);
    }

    gchar* joinDateAndTimeFormatStrings (const char * date_string, const char * time_string) const
    {
        gchar * str;

        if (date_string && time_string)
        {
            /* TRANSLATORS: This is a format string passed to strftime to combine the
             * date and the time.  The value of "%s\u2003%s" will result in a
             * string like this in US English 12-hour time: 'Fri Jul 16 11:50 AM'.
             * The space in between date and time is a Unicode en space
             * (E28082 in UTF-8 hex). */
            str =  g_strdup_printf ("%s\u2003%s", date_string, time_string);
        }
        else if (date_string)
        {
            str = g_strdup (date_string);
        }
        else // time_string
        {
            str = g_strdup (time_string);
        }

        return str;
    }

private:

    DesktopFormatter * const owner_;
    std::shared_ptr<Clock> clock_;
    GSettings * settings_;
};

/***
****
***/

DesktopFormatter::DesktopFormatter (const std::shared_ptr<Clock>& clock):
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
