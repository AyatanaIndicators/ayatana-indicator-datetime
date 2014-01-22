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

#include <datetime/clock.h>

#include <glib.h>
#include <glib/gi18n.h>

#include <locale.h> // setlocale()
#include <langinfo.h> // nl_langinfo()
#include <string.h> // strstr()

namespace unity {
namespace indicator {
namespace datetime {

/***
****
***/

namespace
{

void clear_timer(guint& tag)
{
    if (tag)
    {
        g_source_remove(tag);
        tag = 0;
    }
}

gint calculate_milliseconds_until_next_second(const DateTime& now)
{
    gint interval_usec;
    guint interval_msec;

    interval_usec = G_USEC_PER_SEC - g_date_time_get_microsecond(now.get());
    interval_msec = (interval_usec + 999) / 1000;
    return interval_msec;
}

/*
 * We periodically rebuild the sections that have time format strings
 * that are dependent on the current time:
 *
 * 1. appointment menuitems' time format strings depend on the
 *    current time; for example, they don't show the day of week
 *    if the appointment is today.
 *
 * 2. location menuitems' time format strings depend on the
 *    current time; for example, they don't show the day of the week
 *    if the local date and location date are the same.
 *
 * 3. the "local date" menuitem in the calendar section is,
 *    obviously, dependent on the local time.
 *
 * In short, we want to update whenever the number of days between two zone
 * might have changed. We do that by updating when either zone's day changes.
 *
 * Since not all UTC offsets are evenly divisible by hours
 * (examples: Newfoundland UTC-03:30, Nepal UTC+05:45), refreshing on the hour
 * is not enough. We need to refresh at HH:00, HH:15, HH:30, and HH:45.
 */
guint calculate_seconds_until_next_fifteen_minutes(GDateTime * now)
{
    char * str;
    gint minute;
    guint seconds;
    GTimeSpan diff;
    GDateTime * next;
    GDateTime * start_of_next;

    minute = g_date_time_get_minute(now);
    minute = 15 - (minute % 15);
    next = g_date_time_add_minutes(now, minute);
    start_of_next = g_date_time_new_local(g_date_time_get_year(next),
                                          g_date_time_get_month(next),
                                          g_date_time_get_day_of_month(next),
                                          g_date_time_get_hour(next),
                                          g_date_time_get_minute(next),
                                          0.1);

    str = g_date_time_format(start_of_next, "%F %T");
    g_debug("%s %s the next timestamp rebuild will be at %s", G_STRLOC, G_STRFUNC, str);
    g_free(str);

    diff = g_date_time_difference(start_of_next, now);
    seconds = (diff + (G_TIME_SPAN_SECOND-1)) / G_TIME_SPAN_SECOND;

    g_date_time_unref(start_of_next);
    g_date_time_unref(next);
    return seconds;
}
} // unnamed namespace


class Formatter::Impl
{
public:

    Impl(Formatter* owner, const std::shared_ptr<Clock>& clock):
        m_owner(owner),
        m_clock(clock)
    {
        m_owner->headerFormat.changed().connect([this](const std::string& /*fmt*/){update_header();});
        m_clock->minuteChanged.connect([this](){update_header();});
        update_header();

        restartRelativeTimer();
    }

    ~Impl()
    {
        clear_timer(m_header_seconds_timer);
        clear_timer(m_relative_timer);
    }

private:

    static bool format_shows_seconds(const std::string& fmt)
    {
        return (fmt.find("%s") != std::string::npos)
            || (fmt.find("%S") != std::string::npos)
            || (fmt.find("%T") != std::string::npos)
            || (fmt.find("%X") != std::string::npos)
            || (fmt.find("%c") != std::string::npos);
    }

    void update_header()
    {
        // update the header property
        const auto fmt = m_owner->headerFormat.get();
        const auto str = m_clock->localtime().format(fmt);
        m_owner->header.set(str);

        // if the header needs to show seconds, set a timer.
        if (format_shows_seconds(fmt))
            start_header_timer();
        else
            clear_timer(m_header_seconds_timer);
    }

    // we've got a header format that shows seconds,
    // so we need to update it every second
    void start_header_timer()
    {
        clear_timer(m_header_seconds_timer);

        const auto now = m_clock->localtime();
        auto interval_msec = calculate_milliseconds_until_next_second(now);
        interval_msec += 50; // add a small margin to ensure the callback
                             // fires /after/ next is reached
        m_header_seconds_timer = g_timeout_add_full(G_PRIORITY_HIGH,
                                                    interval_msec,
                                                    on_header_timer,
                                                    this,
                                                    nullptr);
    }

    static gboolean on_header_timer(gpointer gself)
    {
        static_cast<Formatter::Impl*>(gself)->update_header();
        return G_SOURCE_REMOVE;
    }

private:

    void restartRelativeTimer()
    {
        clear_timer(m_relative_timer);

        const auto now = m_clock->localtime();
        const auto seconds = calculate_seconds_until_next_fifteen_minutes(now.get());
        m_relative_timer = g_timeout_add_seconds(seconds, onRelativeTimer, this);
    }

    static gboolean onRelativeTimer(gpointer gself)
    {
        auto self = static_cast<Formatter::Impl*>(gself);
        self->m_owner->relativeFormatChanged();
        self->restartRelativeTimer();
        return G_SOURCE_REMOVE;
    }

private:
    Formatter* const m_owner;
    guint m_header_seconds_timer = 0;
    guint m_relative_timer = 0;

public:
    std::shared_ptr<Clock> m_clock;
};

/***
****
***/

Formatter::Formatter(const std::shared_ptr<Clock>& clock):
    p(new Formatter::Impl(this, clock))
{
}

Formatter::~Formatter()
{
}

bool
Formatter::is_locale_12h()
{
    static const char *formats_24h[] = {"%H", "%R", "%T", "%OH", "%k"};
    const auto t_fmt = nl_langinfo(T_FMT);

    for (const auto& needle : formats_24h)
        if (strstr(t_fmt, needle))
            return false;

    return true;
}

const char*
Formatter::T_(const char *msg)
{
    /* General strategy here is to make sure LANGUAGE is empty (since that
       trumps all LC_* vars) and then to temporarily swap LC_TIME and
       LC_MESSAGES.  Then have gettext translate msg.

       We strdup the strings because the setlocale & *env functions do not
       guarantee anything about the storage used for the string, and thus
       the string may not be portably safe after multiple calls.

       Note that while you might think g_dcgettext would do the trick here,
       that actually looks in /usr/share/locale/XX/LC_TIME, not the
       LC_MESSAGES directory, so we won't find any translation there.
     */

    auto message_locale = g_strdup(setlocale(LC_MESSAGES, nullptr));
    const auto time_locale = setlocale(LC_TIME, nullptr);
    auto language = g_strdup(g_getenv("LANGUAGE"));

    if (language)
        g_unsetenv("LANGUAGE");
    setlocale(LC_MESSAGES, time_locale);

    /* Get the LC_TIME version */
    const auto rv = _(msg);

    /* Put everything back the way it was */
    setlocale(LC_MESSAGES, message_locale);
    if (language)
        g_setenv("LANGUAGE", language, TRUE);

    g_free(message_locale);
    g_free(language);
    return rv;
}

const char*
Formatter::getDefaultHeaderTimeFormat(bool twelvehour, bool show_seconds)
{
  const char* fmt;

  if (twelvehour && show_seconds)
    /* TRANSLATORS: a strftime(3) format for 12hr time w/seconds */
    fmt = T_("%l:%M:%S %p");
  else if (twelvehour)
    /* TRANSLATORS: a strftime(3) format for 12hr time */
    fmt = T_("%l:%M %p");
  else if (show_seconds)
    /* TRANSLATORS: a strftime(3) format for 24hr time w/seconds */
    fmt = T_("%H:%M:%S");
  else
    /* TRANSLATORS: a strftime(3) format for 24hr time */
    fmt = T_("%H:%M");

  return fmt;
}

/***
****
***/

namespace
{
typedef enum
{
    DATE_PROXIMITY_TODAY,
    DATE_PROXIMITY_TOMORROW,
    DATE_PROXIMITY_WEEK,
    DATE_PROXIMITY_FAR
}
date_proximity_t;

date_proximity_t getDateProximity(GDateTime* now, GDateTime* time)
{
    auto prox = DATE_PROXIMITY_FAR;
    gint now_year, now_month, now_day;
    gint time_year, time_month, time_day;

    // does it happen today?
    g_date_time_get_ymd(now, &now_year, &now_month, &now_day);
    g_date_time_get_ymd(time, &time_year, &time_month, &time_day);
    if ((now_year == time_year) && (now_month == time_month) && (now_day == time_day))
        prox = DATE_PROXIMITY_TODAY;

    // does it happen tomorrow?
    if (prox == DATE_PROXIMITY_FAR)
    {
        auto tomorrow = g_date_time_add_days(now, 1);

        gint tom_year, tom_month, tom_day;
        g_date_time_get_ymd(tomorrow, &tom_year, &tom_month, &tom_day);
        if ((tom_year == time_year) && (tom_month == time_month) && (tom_day == time_day))
            prox = DATE_PROXIMITY_TOMORROW;

        g_date_time_unref(tomorrow);
    }

    // does it happen this week?
    if (prox == DATE_PROXIMITY_FAR)
    {
        auto week = g_date_time_add_days(now, 6);
        auto week_bound = g_date_time_new_local(g_date_time_get_year(week),
                                                g_date_time_get_month(week),
                                                g_date_time_get_day_of_month(week),
                                                23, 59, 59.9);

        if (g_date_time_compare(time, week_bound) <= 0)
            prox = DATE_PROXIMITY_WEEK;

        g_date_time_unref(week_bound);
        g_date_time_unref(week);
    }

    return prox;
}
} // unnamed namespace

/**
 * _ a time today should be shown as just the time (e.g. “3:55 PM”)
 * _ a full-day event today should be shown as “Today”
 * _ a time any other day this week should be shown as the short version of the
 *   day and time (e.g. “Wed 3:55 PM”)
 * _ a full-day event tomorrow should be shown as “Tomorrow”
 * _ a full-day event another day this week should be shown as the
 *   weekday (e.g. “Friday”)
 * _ a time after this week should be shown as the short version of the day,
 *   date, and time (e.g. “Wed 21 Apr 3:55 PM”)
 * _ a full-day event after this week should be shown as the short version of
 *   the day and date (e.g. “Wed 21 Apr”). 
 * _ in addition, when presenting the times of upcoming events, the time should
 *   be followed by the timezone if it is different from the one the computer
 *   is currently set to. For example, “Wed 3:55 PM UTC−5”. 
 */
std::string
Formatter::getRelativeFormat(GDateTime* then, GDateTime* then_end) const
{
    std::string ret;
    const auto now = p->m_clock->localtime().get();

    if (then != nullptr)
    {
        const bool full_day = then_end && (g_date_time_difference(then_end, then) >= G_TIME_SPAN_DAY);
        const auto prox = getDateProximity(now, then);

        if (full_day)
        {
            switch (prox)
            {
                case DATE_PROXIMITY_TODAY:    ret = _("Today"); break;
                case DATE_PROXIMITY_TOMORROW: ret = _("Tomorrow"); break;
                case DATE_PROXIMITY_WEEK:     ret = T_("%A"); break;
                case DATE_PROXIMITY_FAR:      ret = T_("%a %d %b"); break;
            }
        }
        else if (is_locale_12h())
        {
            switch (prox)
            {
                case DATE_PROXIMITY_TODAY:    ret = T_("%l:%M %p"); break;
                case DATE_PROXIMITY_TOMORROW: ret = T_("Tomorrow\u2003%l:%M %p"); break;
                case DATE_PROXIMITY_WEEK:     ret = T_("%a\u2003%l:%M %p"); break;
                case DATE_PROXIMITY_FAR:      ret = T_("%a %d %b\u2003%l:%M %p"); break;
            }
        }
        else
        {
            switch (prox)
            {
                case DATE_PROXIMITY_TODAY:    ret = T_("%H:%M"); break;
                case DATE_PROXIMITY_TOMORROW: ret = T_("Tomorrow\u2003%H:%M"); break;
                case DATE_PROXIMITY_WEEK:     ret = T_("%a\u2003%H:%M"); break;
                case DATE_PROXIMITY_FAR:      ret = T_("%a %d %b\u2003%H:%M"); break;
            }
        }

        /* if it's an appointment in a different timezone (and doesn't run for a full day)
           then the time should be followed by its timezone. */
        if ((then_end != nullptr) &&
            (!full_day) &&
            ((g_date_time_get_utc_offset(now) != g_date_time_get_utc_offset(then))))
        {
            ret += ' ';
            ret += g_date_time_get_timezone_abbreviation(then);
        }
    }

    return ret;
}

/***
****
***/

} // namespace datetime
} // namespace indicator
} // namespace unity
