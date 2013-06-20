/*
 * Copyright 2013 Canonical Ltd.
 *
 * Authors:
 *   Charles Kerr <charles.kerr@canonical.com>
 *   Ted Gould <ted@canonical.com>
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
 */

#include "config.h"

#include <string.h> /* strstr() */

#include <glib/gi18n.h>
#include <gio/gio.h>

#include "dbus-shared.h"
#include "planner-eds.h"
#include "timezone-file.h"
#include "timezone-geoclue.h"
#include "service.h"
#include "settings-shared.h"
#include "utils.h"

#define SKEW_CHECK_INTERVAL_SEC 10
#define SKEW_DIFF_THRESHOLD_USEC ((SKEW_CHECK_INTERVAL_SEC+5) * G_USEC_PER_SEC)

G_DEFINE_TYPE (IndicatorDatetimeService,
               indicator_datetime_service,
               G_TYPE_OBJECT)

enum
{
  SIGNAL_NAME_LOST,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

enum
{
  SECTION_HEADER        = (1<<0),
  SECTION_CALENDAR      = (1<<1),
  SECTION_APPOINTMENTS  = (1<<2),
  SECTION_LOCATIONS     = (1<<3),
  SECTION_SETTINGS      = (1<<4),
};

enum
{
  PROFILE_DESKTOP,
  PROFILE_GREETER,
  N_PROFILES
};

static const char * const menu_names[N_PROFILES] =
{
  "desktop",
  "greeter"
};

struct ProfileMenuInfo
{
  /* the root level -- the header is the only child of this */
  GMenu * menu;

  /* parent of the sections. This is the header's submenu */
  GMenu * submenu;

  guint export_id;
};

struct _IndicatorDatetimeServicePrivate
{
  GCancellable * cancellable;

  GSettings * settings;

  IndicatorDatetimeTimezone * tz_file;
  IndicatorDatetimeTimezone * tz_geoclue;
  IndicatorDatetimePlanner * planner;

  guint own_id;
  guint actions_export_id;
  GDBusConnection * conn;

  guint rebuild_id;
  int rebuild_flags;
  struct ProfileMenuInfo menus[N_PROFILES];

  GDateTime * skew_time;
  guint skew_timer;

  guint header_timer;
  guint timezone_timer;

  /* Which year/month to show in the calendar,
     and which day should get the cursor.
     This value is reflected in the calendar action's state */
  GDateTime * calendar_date;

  GSimpleActionGroup * actions;
  GSimpleAction * header_action;
  GSimpleAction * calendar_action;

  GDBusProxy * login1_manager;
};

typedef IndicatorDatetimeServicePrivate priv_t;

/***
****
***/

static void
indicator_clear_timer (guint * tag)
{
  if (*tag)
    {
      g_source_remove (*tag);
      *tag = 0;
    }
}

/***
****
***/

static void rebuild_now (IndicatorDatetimeService * self, int section);
static void rebuild_soon (IndicatorDatetimeService * self, int section);

static inline void
rebuild_header_soon (IndicatorDatetimeService * self)
{
  rebuild_soon (self, SECTION_HEADER);
}

static inline void
rebuild_calendar_section_soon (IndicatorDatetimeService * self)
{
  rebuild_soon (self, SECTION_CALENDAR);
}

static inline void
rebuild_appointments_section_soon (IndicatorDatetimeService * self)
{
  rebuild_soon (self, SECTION_APPOINTMENTS);
}

static inline void
rebuild_locations_section_soon (IndicatorDatetimeService * self)
{
  rebuild_soon (self, SECTION_LOCATIONS);
}

static inline void
rebuild_settings_section_soon (IndicatorDatetimeService * self)
{
  rebuild_soon (self, SECTION_SETTINGS);
}

/***
****  TIMEZONE TIMER
***/

/*
 * Periodically rebuild the sections that have time format strings
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

static guint
calculate_seconds_until_next_fifteen_minutes (GDateTime * now)
{
  char * str;
  gint minute;
  guint seconds;
  GTimeSpan diff;
  GDateTime * next;
  GDateTime * start_of_next;

  minute = g_date_time_get_minute (now);
  minute = 15 - (minute % 15);
  next = g_date_time_add_minutes (now, minute);
  start_of_next = g_date_time_new_local (g_date_time_get_year (next),
                                         g_date_time_get_month (next),
                                         g_date_time_get_day_of_month (next),
                                         g_date_time_get_hour (next),
                                         g_date_time_get_minute (next),
                                         1);

  str = g_date_time_format (start_of_next, "%F %T");
  g_debug ("%s %s the next timestamp rebuild will be at %s", G_STRLOC, G_STRFUNC, str);
  g_free (str);

  diff = g_date_time_difference (start_of_next, now);
  seconds = (diff + (G_TIME_SPAN_SECOND-1)) / G_TIME_SPAN_SECOND;

  g_date_time_unref (start_of_next);
  g_date_time_unref (next);

  return seconds;
}

static void start_timezone_timer (IndicatorDatetimeService * self);

static gboolean
on_timezone_timer (gpointer gself)
{
  IndicatorDatetimeService * self = INDICATOR_DATETIME_SERVICE (gself);

  rebuild_soon (self, SECTION_CALENDAR |
                      SECTION_APPOINTMENTS |
                      SECTION_LOCATIONS);

  /* Restarting the timer to recalculate the interval. This helps us to hit
     our marks despite clock skew, suspend+resume, leap seconds, etc */
  start_timezone_timer (self);
  return G_SOURCE_REMOVE;
}

static void
start_timezone_timer (IndicatorDatetimeService * self)
{
  GDateTime * now;
  guint seconds;
  priv_t * p = self->priv;

  indicator_clear_timer (&p->timezone_timer);

  now = indicator_datetime_service_get_localtime (self);
  seconds = calculate_seconds_until_next_fifteen_minutes (now);
  p->timezone_timer = g_timeout_add_seconds (seconds, on_timezone_timer, self);
  g_date_time_unref (now);
}

/***
****  HEADER TIMER
***/

/*
 * This is to periodically rebuild the header's action's state.
 *
 * If the label shows seconds, update when we reach the next second.
 * Otherwise, update when we reach the next minute.
 */

static guint
calculate_milliseconds_until_next_minute (GDateTime * now)
{
  GDateTime * next;
  GDateTime * start_of_next;
  GTimeSpan interval_usec;
  guint interval_msec;

  next = g_date_time_add_minutes (now, 1);
  start_of_next = g_date_time_new_local (g_date_time_get_year (next),
                                         g_date_time_get_month (next),
                                         g_date_time_get_day_of_month (next),
                                         g_date_time_get_hour (next),
                                         g_date_time_get_minute (next),
                                         0);

  interval_usec = g_date_time_difference (start_of_next, now);
  interval_msec = (interval_usec + 999) / 1000;

  g_date_time_unref (start_of_next);
  g_date_time_unref (next);

  return interval_msec;
}

static gint
calculate_milliseconds_until_next_second (GDateTime * now)
{
  gint interval_usec;
  guint interval_msec;

  interval_usec = G_USEC_PER_SEC - g_date_time_get_microsecond (now);
  interval_msec = (interval_usec + 999) / 1000;

  return interval_msec;
}

static void start_header_timer (IndicatorDatetimeService * self);

static gboolean
on_header_timer (gpointer gself)
{
  IndicatorDatetimeService * self = INDICATOR_DATETIME_SERVICE (gself);

  rebuild_now (self, SECTION_HEADER);

  /* Restarting the timer to recalculate the interval. This helps us to hit
     our marks despite clock skew, suspend+resume, leap seconds, etc */
  start_header_timer (self);
  return G_SOURCE_REMOVE;
}

static char * get_header_label_format_string (IndicatorDatetimeService *);

static void
start_header_timer (IndicatorDatetimeService * self)
{
  guint interval_msec;
  gboolean header_shows_seconds = FALSE;
  priv_t * p = self->priv;
  GDateTime * now = indicator_datetime_service_get_localtime (self);
 
  indicator_clear_timer (&p->header_timer);

  if (g_settings_get_boolean (self->priv->settings, SETTINGS_SHOW_CLOCK_S))
    {
      char * fmt = get_header_label_format_string (self);
      header_shows_seconds = fmt && (strstr(fmt,"%s") || strstr(fmt,"%S") ||
                                     strstr(fmt,"%T") || strstr(fmt,"%X") ||
                                     strstr(fmt,"%c"));
      g_free (fmt);
    }

  if (header_shows_seconds)
    interval_msec = calculate_milliseconds_until_next_second (now);
  else
    interval_msec = calculate_milliseconds_until_next_minute (now);

  interval_msec += 50; /* add a small margin to ensure the callback
                          fires /after/ next is reached */

  p->header_timer = g_timeout_add_full (G_PRIORITY_HIGH,
                                        interval_msec,
                                        on_header_timer,
                                        self,
                                        NULL);

  g_date_time_unref (now);
}

/**
 * General purpose handler for rebuilding sections and restarting their timers
 * when time jumps for whatever reason:
 *
 *  - clock skew
 *  - laptop suspend + resume
 *  - geoclue detects that we've changed timezones
 *  - Unity is running inside a TARDIS
 */
static void
on_local_time_jumped (IndicatorDatetimeService * self)
{
  g_debug ("%s %s", G_STRLOC, G_STRFUNC);

  /* these calls accomplish two things:
     1. rebuild the necessary states / menuitems when time jumps
     2. restart the timers so their new wait interval is correct */

  on_header_timer (self);
  on_timezone_timer (self);
}

static gboolean
skew_timer_func (gpointer gself)
{
  IndicatorDatetimeService * self = INDICATOR_DATETIME_SERVICE (gself);
  GDateTime * now = indicator_datetime_service_get_localtime (self);
  priv_t * p = self->priv;

  /* check for clock skew: has too much time passed since the last check? */
  if (p->skew_time != NULL)
    {
      const GTimeSpan diff = g_date_time_difference (now, p->skew_time);

      if (diff > SKEW_DIFF_THRESHOLD_USEC)
        on_local_time_jumped (self);
    }

  g_clear_pointer (&p->skew_time, g_date_time_unref);
  p->skew_time = now;
  return G_SOURCE_CONTINUE;
}

/***
****
****  HEADER SECTION
****
***/

typedef enum 
{
  TIME_FORMAT_MODE_LOCALE_DEFAULT,
  TIME_FORMAT_MODE_12_HOUR,
  TIME_FORMAT_MODE_24_HOUR,
  TIME_FORMAT_MODE_CUSTOM
}
TimeFormatMode;

/* gets the user's time-format from GSettings */
static TimeFormatMode
get_time_format_mode (IndicatorDatetimeService * self)
{
  char * str;
  TimeFormatMode mode;

  str = g_settings_get_string (self->priv->settings, SETTINGS_TIME_FORMAT_S);

  if (!g_strcmp0 ("12-hour", str))
    mode = TIME_FORMAT_MODE_12_HOUR;
  else if (!g_strcmp0 ("24-hour", str))
    mode = TIME_FORMAT_MODE_24_HOUR;
  else if (!g_strcmp0 ("custom", str))
    mode = TIME_FORMAT_MODE_CUSTOM;
  else
    mode = TIME_FORMAT_MODE_LOCALE_DEFAULT;

  g_free (str);
  return mode;
}

static gchar *
get_header_label_format_string (IndicatorDatetimeService * self)
{
  char * fmt;
  const TimeFormatMode mode = get_time_format_mode (self);
  GSettings * s = self->priv->settings;

  if (mode == TIME_FORMAT_MODE_CUSTOM)
    {
      fmt = g_settings_get_string (s, SETTINGS_CUSTOM_TIME_FORMAT_S);
    }
  else
    {
      gboolean show_day = g_settings_get_boolean (s, SETTINGS_SHOW_DAY_S);
      gboolean show_date = g_settings_get_boolean (s, SETTINGS_SHOW_DATE_S);
      fmt = generate_format_string_full (show_day, show_date);
    }

  return fmt;
}

static GVariant *
create_header_state (IndicatorDatetimeService * self)
{
  GVariantBuilder b;
  gchar * fmt;
  gchar * str;
  gboolean visible;
  GDateTime * now;
  priv_t * p = self->priv;

  visible = g_settings_get_boolean (p->settings, SETTINGS_SHOW_CLOCK_S);

  /* build the time string for the label & a11y */
  fmt = get_header_label_format_string (self);
  now = indicator_datetime_service_get_localtime (self);
  str = g_date_time_format (now, fmt);
  if (str == NULL)
    {
      str = g_strdup (_("Unsupported date format"));
      g_warning ("%s", str);
    }

  g_variant_builder_init (&b, G_VARIANT_TYPE("a{sv}"));
  g_variant_builder_add (&b, "{sv}", "accessible-desc", g_variant_new_string (str));
  g_variant_builder_add (&b, "{sv}", "label", g_variant_new_string (str));
  g_variant_builder_add (&b, "{sv}", "visible", g_variant_new_boolean (visible));

  /* cleanup */
  g_date_time_unref (now);
  g_free (str);
  g_free (fmt);
  return g_variant_builder_end (&b);
}


/***
****
****  CALENDAR SECTION
****
***/

static GDateTime *
get_calendar_date (IndicatorDatetimeService * self)
{
  GDateTime * date;
  priv_t * p = self->priv;

  if (p->calendar_date)
    date = g_date_time_ref (p->calendar_date);
  else
    date = indicator_datetime_service_get_localtime (self);

  return date;
}

static GSList *
get_all_appointments_this_month (IndicatorDatetimeService * self)
{
  GSList * appointments = NULL;
  priv_t * p = self->priv;

  if (p->planner != NULL)
    {
      GDateTime * calendar_date;
      GDateTime * begin;
      GDateTime * end;
      int y, m, d;

      calendar_date = get_calendar_date (self);
      g_date_time_get_ymd (calendar_date, &y, &m, &d);
      begin = g_date_time_new_local (y, m, 0,
                                     0, 0, 0);
      end = g_date_time_new_local (y, m, g_date_get_days_in_month(m,y),
                                   23, 59, 0);

      appointments = indicator_datetime_planner_get_appointments (p->planner,
                                                                  begin,
                                                                  end);

      g_date_time_unref (end);
      g_date_time_unref (begin);
      g_date_time_unref (calendar_date);
    }

  return appointments;
}

static GVariant *
create_calendar_state (IndicatorDatetimeService * self)
{
  guint i;
  const char * key;
  gboolean days[32] = { 0 };
  GVariantBuilder dict_builder;
  GVariantBuilder day_builder;
  GDateTime * date;
  GSList * l;
  GSList * appts;
  gboolean b;
  priv_t * p = self->priv;

  g_variant_builder_init (&dict_builder, G_VARIANT_TYPE_DICTIONARY);

  key = "appointment-days";
  appts = get_all_appointments_this_month (self);
  for (l=appts; l!=NULL; l=l->next)
    {
      const struct IndicatorDatetimeAppt * appt = l->data;
      days[g_date_time_get_day_of_month (appt->begin)] = TRUE;
    }
  g_variant_builder_init (&day_builder, G_VARIANT_TYPE("ai"));
  for (i=0; i<G_N_ELEMENTS(days); i++)
    if (days[i])
      g_variant_builder_add (&day_builder, "i", i);
  g_variant_builder_add (&dict_builder, "{sv}", key,
                         g_variant_builder_end (&day_builder));
  g_slist_free_full (appts, (GDestroyNotify)indicator_datetime_appt_free);

  key = "calendar-day";
  date = get_calendar_date (self);
  g_variant_builder_add (&dict_builder, "{sv}", key,
                         g_variant_new_int64 (g_date_time_to_unix (date)));
  g_date_time_unref (date);

  key = "show-week-numbers";
  b = g_settings_get_boolean (p->settings, SETTINGS_SHOW_WEEK_NUMBERS_S);
  g_variant_builder_add (&dict_builder, "{sv}", key, g_variant_new_boolean (b));

  return g_variant_builder_end (&dict_builder);
}

static void
update_calendar_action_state (IndicatorDatetimeService * self)
{
  g_simple_action_set_state (self->priv->calendar_action,
                             create_calendar_state (self));
}

static GMenuModel *
create_calendar_section (IndicatorDatetimeService * self)
{
  char * label;
  GMenuItem * menu_item;
  GDateTime * now;
  GMenu * menu = g_menu_new ();

  /* create the local date menuitem */
  now = indicator_datetime_service_get_localtime (self);
  label = g_date_time_format (now, _("%A, %e %B %Y"));
  menu_item = g_menu_item_new (label, NULL);
  g_menu_item_set_action_and_target_value (menu_item, "indicator.activate-planner",
                                           g_variant_new_int64(0));
  g_menu_append_item (menu, menu_item);
  g_object_unref (menu_item);
  g_free (label);
  g_date_time_unref (now);

  /* create the calendar menuitem */
  if (g_settings_get_boolean (self->priv->settings, SETTINGS_SHOW_CALENDAR_S))
    {
      label = g_strdup ("[calendar]");
      menu_item = g_menu_item_new ("[calendar]", NULL);
      g_menu_item_set_action_and_target_value (menu_item,
                                               "indicator.calendar",
                                               g_variant_new_int64(0));
      g_menu_item_set_attribute (menu_item, "x-canonical-type",
                                 "s", "com.canonical.indicator.calendar");
      g_menu_item_set_attribute (menu_item, "activation-action",
                                 "s", "indicator.activate-planner");
      g_menu_append_item (menu, menu_item);
      g_object_unref (menu_item);
      g_free (label);
    }

  return G_MENU_MODEL (menu);
}

/***
****
****  APPOINTMENTS SECTION
****
***/

/* gets the next MAX_APPTS appointments */
static GSList *
get_upcoming_appointments (IndicatorDatetimeService * self)
{
  const int MAX_APPTS = 5;
  GSList * l;
  GSList * appts = NULL;
  priv_t * p = self->priv;

  if (p->planner != NULL)
    {
      GDateTime * begin = get_calendar_date (self);
      GDateTime * end = g_date_time_add_months (begin, 1);

      appts = indicator_datetime_planner_get_appointments (p->planner,
                                                           begin,
                                                           end);

      g_date_time_unref (end);
      g_date_time_unref (begin);
    }

  /* truncate at MAX_APPTS */
  if ((l = g_slist_nth (appts, MAX_APPTS-1)))
    {
      g_slist_free_full (l->next, (GDestroyNotify)indicator_datetime_appt_free);
      l->next = NULL;
    }

  return appts;
}

static char *
get_appointment_time_format (struct IndicatorDatetimeAppt  * appt, GDateTime * now)
{
  char * fmt;
  gboolean full_day = g_date_time_difference (appt->end, appt->begin) == G_TIME_SPAN_DAY;

  if (full_day)
    {
      /* TRANSLATORS: This is a strftime string for the day for full day events
         in the menu.  It should most likely be either '%A' for a full text day
         (Wednesday) or '%a' for a shortened one (Wed).  You should only need to
         change for '%a' in the case of langauges with very long day names. */
      fmt = g_strdup (_("%A"));
    }
  else
    {
      fmt = generate_format_string_at_time (now, appt->begin);
    }

  return fmt;
}

static GMenuModel *
create_appointments_section (IndicatorDatetimeService * self)
{
  priv_t * p = self->priv;
  GMenu * menu = g_menu_new ();

  if (g_settings_get_boolean (p->settings, SETTINGS_SHOW_EVENTS_S))
    {
      GSList * l;
      GSList * appts;
      GMenuItem * menu_item;
      GDateTime * now = indicator_datetime_service_get_localtime (self);

      /* build appointment menuitems */
      appts = get_upcoming_appointments (self);
      for (l=appts; l!=NULL; l=l->next)
        {
          struct IndicatorDatetimeAppt * appt = l->data;
          char * fmt = get_appointment_time_format (appt, now);
          const gint64 unix_time = g_date_time_to_unix (appt->begin);

          menu_item = g_menu_item_new (appt->summary, NULL);
          g_menu_item_set_attribute (menu_item, "x-canonical-color",
                                     "s", appt->color);
          g_menu_item_set_attribute (menu_item, "x-canonical-time",
                                     "x", unix_time);
          g_menu_item_set_attribute (menu_item, "x-canonical-time-format",
                                     "s", fmt);
          g_menu_item_set_attribute (menu_item, "x-canonical-type",
                                     "s", "com.canonical.indicator.appointment");
          g_menu_item_set_action_and_target_value (menu_item,
                                                   "indicator.activate-planner",
                                                   g_variant_new_int64 (unix_time));
          g_menu_append_item (menu, menu_item);
          g_object_unref (menu_item);
          g_free (fmt);
        }

      /* build 'add event' menuitem */
      menu_item = g_menu_item_new (_("Add Event…"), NULL);
      g_menu_item_set_action_and_target_value (menu_item,
                                               "indicator.activate-planner",
                                               g_variant_new_int64 (0));
      g_menu_append_item (menu, menu_item);
      g_object_unref (menu_item);

      /* cleanup */
      g_date_time_unref (now);
      g_slist_free_full (appts, (GDestroyNotify)indicator_datetime_appt_free);
    }

  return G_MENU_MODEL (menu);
}


/***
****
****  LOCATIONS SECTION
****
***/

static void
on_current_timezone_changed (IndicatorDatetimeService * self)
{
  on_local_time_jumped (self);
}

/* When the 'auto-detect timezone' boolean setting changes,
   start or stop watching geoclue and /etc/timezone */
static void
set_detect_location_enabled (IndicatorDatetimeService * self, gboolean enabled)
{
  gboolean changed = FALSE;
  priv_t * p = self->priv;

  /* geoclue */

  if (!p->tz_geoclue && enabled)
    {
      p->tz_geoclue = indicator_datetime_timezone_geoclue_new ();
      g_signal_connect_swapped (p->tz_geoclue, "notify::timezone",
                                G_CALLBACK(on_current_timezone_changed),
                                self);
      changed = TRUE;
    }
  else if (p->tz_geoclue && !enabled)
    {
      g_signal_handlers_disconnect_by_func (p->tz_geoclue,
                                            on_current_timezone_changed,
                                            self);
      g_clear_object (&p->tz_geoclue);
      changed = TRUE;
    }

  /* timezone file */

  if (!p->tz_file && enabled)
    {
      p->tz_file = indicator_datetime_timezone_file_new (TIMEZONE_FILE);
      g_signal_connect_swapped (p->tz_file, "notify::timezone",
                                G_CALLBACK(on_current_timezone_changed),
                                self);
      changed = TRUE;
    }
  else if (p->tz_file && !enabled)
    {
      g_signal_handlers_disconnect_by_func (p->tz_file,
                                            on_current_timezone_changed,
                                            self);
      g_clear_object (&p->tz_file);
      changed = TRUE;
    }

  if (changed)
    on_current_timezone_changed (self);
}

/* A temp struct used by create_locations_section()
   for pruning duplicates and sorting. */
struct TimeLocation
{
  GTimeSpan offset;
  gchar * zone;
  gchar * name;
  gboolean visible;
  GDateTime * local_time;
};

static void
time_location_free (struct TimeLocation * loc)
{
  g_date_time_unref (loc->local_time);
  g_free (loc->name);
  g_free (loc->zone);
  g_free (loc);
}

static struct TimeLocation*
time_location_new (const char * zone,
                   const char * name,
                   gboolean     visible)
{
  struct TimeLocation * loc = g_new (struct TimeLocation, 1);
  GTimeZone * tz = g_time_zone_new (zone);
  loc->zone = g_strdup (zone);
  loc->name = g_strdup (name);
  loc->visible = visible;
  loc->local_time = g_date_time_new_now (tz);
  loc->offset = g_date_time_get_utc_offset (loc->local_time);
  g_time_zone_unref (tz);
  return loc;
}

static int
time_location_compare (const struct TimeLocation * a,
                       const struct TimeLocation * b)
{
  int ret = 0;

  if (!ret && (a->offset != b->offset)) /* primary key */
    ret = (a->offset < b->offset) ? -1 : 1;

  if (!ret)
    ret = g_strcmp0 (a->name, b->name); /* secondary key */

  if (!ret)
    ret = a->visible - b->visible; /* tertiary key */

  return ret;
}

static GSList*
locations_add (GSList     * locations,
               const char * zone,
               const char * name,
               gboolean     visible)
{
  struct TimeLocation * loc = time_location_new (zone, name, visible);

  if (g_slist_find_custom (locations, loc, (GCompareFunc)time_location_compare))
    {
      g_debug("%s Skipping duplicate zone '%s' name '%s'", G_STRLOC, zone, name);
      time_location_free (loc);
    }
  else
    {
      g_debug ("%s Adding zone '%s', name '%s'", G_STRLOC, zone, name);
      locations = g_slist_append (locations, loc);
    }

  return locations;
}

static GMenuModel *
create_locations_section (IndicatorDatetimeService * self)
{
  guint i;
  GMenu * menu;
  GSList * l;
  GSList * locations = NULL;
  gchar ** user_locations;
  gboolean visible;
  IndicatorDatetimeTimezone * detected_timezones[2];
  priv_t * p = self->priv;
  GDateTime * now = indicator_datetime_service_get_localtime (self);

  set_detect_location_enabled (self,
                               g_settings_get_boolean (p->settings, SETTINGS_SHOW_DETECTED_S));

  menu = g_menu_new ();

  /***
  ****  Build a list of locations to add: use geo_timezone,
  ****  current_timezone, and SETTINGS_LOCATIONS_S, but omit duplicates.
  ***/

  /* maybe add the auto-detected timezones */
  detected_timezones[0] = p->tz_geoclue;
  detected_timezones[1] = p->tz_file;
  visible = g_settings_get_boolean (p->settings, SETTINGS_SHOW_DETECTED_S);
  for (i=0; i<G_N_ELEMENTS(detected_timezones); i++)
    {
      if (detected_timezones[i] != NULL)
        {
          const char * tz = indicator_datetime_timezone_get_timezone (detected_timezones[i]);
          if (tz && *tz)
            {
              gchar * name = get_current_zone_name (tz);
              locations = locations_add (locations, tz, name, visible);
              g_free (name);
            }
        }
    }

  /* maybe add the user-specified locations */
  user_locations = g_settings_get_strv (p->settings, SETTINGS_LOCATIONS_S);
  if (user_locations != NULL)
    {
      visible = g_settings_get_boolean (p->settings, SETTINGS_SHOW_LOCATIONS_S);

      for (i=0; user_locations[i] != NULL; i++)
        {
          gchar * zone;
          gchar * name;
          split_settings_location (user_locations[i], &zone, &name);
          locations = locations_add (locations, zone, name, visible);
          g_free (name);
          g_free (zone);
        }

      g_strfreev (user_locations);
      user_locations = NULL;
    }

  /* now build menuitems for all the locations */
  for (l=locations; l!=NULL; l=l->next)
    {
      struct TimeLocation * loc = l->data;
      if (loc->visible)
        {
          char * label;
          char * detailed_action;
          char * fmt;
          GMenuItem * menu_item;

          label = g_strdup (loc->name);
          detailed_action = g_strdup_printf ("indicator.set-location::%s %s",
                                             loc->zone,
                                             loc->name);
          fmt = generate_format_string_at_time (now, loc->local_time);

          menu_item = g_menu_item_new (label, detailed_action);
          g_menu_item_set_attribute (menu_item, "x-canonical-type",
                                     "s", "com.canonical.indicator.location");
          g_menu_item_set_attribute (menu_item, "x-canonical-timezone",
                                     "s", loc->zone);
          g_menu_item_set_attribute (menu_item, "x-canonical-time-format",
                                     "s", fmt);
          g_menu_append_item (menu, menu_item);

          g_object_unref (menu_item);
          g_free (fmt);
          g_free (detailed_action);
          g_free (label);
        }
    }

  g_date_time_unref (now);
  g_slist_free_full (locations, (GDestroyNotify)time_location_free);
  return G_MENU_MODEL (menu);
}

/***
****  SET LOCATION
***/

struct setlocation_data
{
  IndicatorDatetimeService * service;
  char * timezone_id;
  char * name;
};

static void
setlocation_data_free (struct setlocation_data * data)
{
  g_free (data->timezone_id);
  g_free (data->name);
  g_free (data);
}

static void
on_datetime1_set_timezone_response (GObject       * object,
                                    GAsyncResult  * res,
                                    gpointer        gdata)
{
  GError * err;
  GVariant * answers;
  struct setlocation_data * data = gdata;

  err = NULL;
  answers = g_dbus_proxy_call_finish (G_DBUS_PROXY(object), res, &err);
  if (err != NULL)
    {
      if (!g_error_matches (err, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Could not set new timezone: %s", err->message);

      g_error_free (err);
    }
  else
    {
      char * timezone_name = g_strdup_printf ("%s %s",
                                              data->timezone_id,
                                              data->name);

      g_settings_set_string (data->service->priv->settings,
                             SETTINGS_TIMEZONE_NAME_S,
                             timezone_name);

      g_free (timezone_name);
      g_variant_unref (answers);
    }

  setlocation_data_free (data);
}

static void
on_datetime1_proxy_ready (GObject      * object G_GNUC_UNUSED,
                          GAsyncResult * res,
                          gpointer       gdata)
{
  GError * err;
  GDBusProxy * proxy;
  struct setlocation_data * data = gdata;

  err = NULL;
  proxy = g_dbus_proxy_new_for_bus_finish (res, &err);
  if (err != NULL)
    {
      if (!g_error_matches (err, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Could not grab DBus proxy for timedated: %s", err->message);

      g_error_free (err);
      setlocation_data_free (data);
    }
  else
    {
      g_dbus_proxy_call (proxy,
                         "SetTimezone",
                         g_variant_new ("(sb)", data->timezone_id, TRUE),
                         G_DBUS_CALL_FLAGS_NONE,
                         -1,
                         data->service->priv->cancellable,
                         on_datetime1_set_timezone_response,
                         data);

      g_object_unref (proxy);
    }
}

static void
indicator_datetime_service_set_location (IndicatorDatetimeService * self,
                                         const char               * timezone_id,
                                         const char               * name)
{
  priv_t * p = self->priv;
  struct setlocation_data * data;

  g_return_if_fail (INDICATOR_IS_DATETIME_SERVICE (self));
  g_return_if_fail (name && *name);
  g_return_if_fail (timezone_id && *timezone_id);

  data = g_new0 (struct setlocation_data, 1);
  data->timezone_id = g_strdup (timezone_id);
  data->name = g_strdup (name);
  data->service = self;

  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                            G_DBUS_PROXY_FLAGS_NONE,
                            NULL,
                            "org.freedesktop.timedate1",
                            "/org/freedesktop/timedate1",
                            "org.freedesktop.timedate1",
                            p->cancellable,
                            on_datetime1_proxy_ready,
                            data);
}

static void
on_set_location (GSimpleAction * a G_GNUC_UNUSED,
                 GVariant      * param,
                 gpointer        gself)
{
  char * zone;
  char * name;
  IndicatorDatetimeService * self;

  self = INDICATOR_DATETIME_SERVICE (gself);
  split_settings_location (g_variant_get_string (param, NULL), &zone, &name);
  indicator_datetime_service_set_location (self, zone, name);

  g_free (name);
  g_free (zone);
}

/***
****
***/

static GMenuModel *
create_settings_section (IndicatorDatetimeService * self G_GNUC_UNUSED)
{
  GMenu * menu = g_menu_new ();
  g_menu_append (menu, _("Date and Time Settings\342\200\246"), "indicator.activate-settings");
  return G_MENU_MODEL (menu);
}

static void
create_menu (IndicatorDatetimeService * self, int profile)
{
  GMenu * menu;
  GMenu * submenu;
  GMenuItem * header;
  GMenuModel * sections[16];
  int i;
  int n = 0;

  g_assert (0<=profile && profile<N_PROFILES);
  g_assert (self->priv->menus[profile].menu == NULL);

  if (profile == PROFILE_DESKTOP)
    {
      sections[n++] = create_calendar_section (self);
      sections[n++] = create_appointments_section (self);
      sections[n++] = create_locations_section (self);
      sections[n++] = create_settings_section (self);
    }
  else if (profile == PROFILE_GREETER)
    {
      sections[n++] = create_calendar_section (self);
    }

  /* add sections to the submenu */

  submenu = g_menu_new ();

  for (i=0; i<n; ++i)
    {
      g_menu_append_section (submenu, NULL, sections[i]);
      g_object_unref (sections[i]);
    }

  /* add submenu to the header */
  header = g_menu_item_new (NULL, "indicator._header");
  g_menu_item_set_attribute (header, "x-canonical-type",
                             "s", "com.canonical.indicator.root");
  g_menu_item_set_submenu (header, G_MENU_MODEL (submenu));
  g_object_unref (submenu);

  /* add header to the menu */
  menu = g_menu_new ();
  g_menu_append_item (menu, header);
  g_object_unref (header);

  self->priv->menus[profile].menu = menu;
  self->priv->menus[profile].submenu = submenu;
}

/***
****  GActions
***/

/* Run a particular program based on an activation */
static void
execute_command (const gchar * cmd)
{
  GError * err = NULL;

  g_debug ("Issuing command '%s'", cmd);

  if (!g_spawn_command_line_async (cmd, &err))
    {
      g_warning ("Unable to start %s: %s", cmd, err->message);
      g_error_free (err);
    }
}

static void
on_settings_activated (GSimpleAction * a      G_GNUC_UNUSED,
                       GVariant      * param  G_GNUC_UNUSED,
                       gpointer        gself  G_GNUC_UNUSED)
{
#ifdef HAVE_CCPANEL
  execute_command ("gnome-control-center indicator-datetime");
#else
  execute_command ("gnome-control-center datetime");
#endif
}

static void
on_activate_planner (GSimpleAction * a         G_GNUC_UNUSED,
                     GVariant      * param,
                     gpointer        gself)
{
  priv_t * p = INDICATOR_DATETIME_SERVICE(gself)->priv;

  if (p->planner != NULL)
    {
      const gint64 t = g_variant_get_int64 (param);
      if (t)
        {
          GDateTime * date_time = g_date_time_new_from_unix_local (t);
          indicator_datetime_planner_activate_time (p->planner, date_time);
          g_date_time_unref (date_time);
        }
      else /* no time specified... */
        {
          indicator_datetime_planner_activate (p->planner);
        }
    }
}

static void
on_calendar_action_activated (GSimpleAction * action G_GNUC_UNUSED,
                              GVariant      * state,
                              gpointer        gself)
{
  gint64 unix_time;
  IndicatorDatetimeService * self = INDICATOR_DATETIME_SERVICE (gself);

  if ((unix_time = g_variant_get_int64 (state)))
    {
      GDateTime * date = g_date_time_new_from_unix_local (unix_time);
      indicator_datetime_service_set_calendar_date (self, date);
      g_date_time_unref (date);
    }
  else /* unset */
    {
      indicator_datetime_service_set_calendar_date (self, NULL);
    }
}


static void
init_gactions (IndicatorDatetimeService * self)
{
  GSimpleAction * a;
  priv_t * p = self->priv;

  GActionEntry entries[] = {
    { "activate-settings", on_settings_activated },
    { "activate-planner", on_activate_planner, "x", NULL },
    { "set-location", on_set_location, "s" }
  };

  p->actions = g_simple_action_group_new ();

  g_action_map_add_action_entries (G_ACTION_MAP(p->actions),
                                   entries,
                                   G_N_ELEMENTS(entries),
                                   self);

  /* add the header action */
  a = g_simple_action_new_stateful ("_header", NULL, create_header_state (self));
  g_simple_action_group_insert (p->actions, G_ACTION(a));
  p->header_action = a;

  /* add the calendar action */
  a = g_simple_action_new_stateful ("calendar",
                                    G_VARIANT_TYPE_INT64,
                                    create_calendar_state (self));
  g_simple_action_group_insert (p->actions, G_ACTION(a));
  g_signal_connect (a, "activate",
                    G_CALLBACK(on_calendar_action_activated), self);
  p->calendar_action = a;

  rebuild_now (self, SECTION_HEADER);
}

/***
****
***/

/**
 * A small helper function for rebuild_now().
 * - removes the previous section
 * - adds and unrefs the new section
 */
static void
rebuild_section (GMenu * parent, int pos, GMenuModel * new_section)
{
  g_menu_remove (parent, pos);
  g_menu_insert_section (parent, pos, NULL, new_section);
  g_object_unref (new_section);
}

static void
rebuild_now (IndicatorDatetimeService * self, int sections)
{
  priv_t * p = self->priv;
  struct ProfileMenuInfo * desktop = &p->menus[PROFILE_DESKTOP];
  struct ProfileMenuInfo * greeter = &p->menus[PROFILE_GREETER];

  if (sections & SECTION_HEADER)
    {
      g_simple_action_set_state (p->header_action, create_header_state (self));
    }

  if (sections & SECTION_CALENDAR)
    {
      rebuild_section (desktop->submenu, 0, create_calendar_section (self));
      rebuild_section (greeter->submenu, 0, create_calendar_section (self));
    }

  if (sections & SECTION_APPOINTMENTS)
    {
      rebuild_section (desktop->submenu, 1, create_appointments_section (self));
    }

  if (sections & SECTION_LOCATIONS)
    {
      rebuild_section (desktop->submenu, 2, create_locations_section (self));
    }

  if (sections & SECTION_SETTINGS)
    {
      rebuild_section (desktop->submenu, 3, create_settings_section (self));
    }
}

static int
rebuild_timeout_func (IndicatorDatetimeService * self)
{
  priv_t * p = self->priv;
  rebuild_now (self, p->rebuild_flags);
  p->rebuild_flags = 0;
  p->rebuild_id = 0;
  return G_SOURCE_REMOVE;
}

static void
rebuild_soon (IndicatorDatetimeService * self, int section)
{
  priv_t * p = self->priv;

  p->rebuild_flags |= section;

  if (p->rebuild_id == 0)
    {
      /* Change events seem to come over the bus in small bursts. This msec
         value is an arbitrary number that tries to be large enough to fold
         multiple events into a single rebuild, but small enough that the
         user won't notice any lag. */
      static const int REBUILD_INTERVAL_MSEC = 500;

      p->rebuild_id = g_timeout_add (REBUILD_INTERVAL_MSEC,
                                     (GSourceFunc)rebuild_timeout_func,
                                     self);
    }
}

/***
****  org.freedesktop.login1.Manager
***/

static void
on_login1_manager_signal (GDBusProxy  * proxy         G_GNUC_UNUSED,
                          gchar       * sender_name   G_GNUC_UNUSED,
                          gchar       * signal_name,
                          GVariant    * parameters,
                          gpointer      gself)
{
  if (!g_strcmp0 (signal_name, "PrepareForSleep"))
    {
      gboolean sleeping = FALSE;
      g_variant_get (parameters, "(b)", &sleeping);
      if (!sleeping)
        on_local_time_jumped (INDICATOR_DATETIME_SERVICE (gself));
    }
}

static void
on_login1_manager_proxy_ready (GObject       * object  G_GNUC_UNUSED,
                               GAsyncResult  * res,
                               gpointer        gself)
{
  GError * err;
  GDBusProxy * proxy;

  err = NULL;
  proxy = g_dbus_proxy_new_for_bus_finish (res, &err);

  if (err != NULL)
    {
      if (!g_error_matches (err, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Could not grab DBus proxy for logind: %s", err->message);

      g_error_free (err);
    }
  else
    {
      IndicatorDatetimeService * self = INDICATOR_DATETIME_SERVICE (gself);
      self->priv->login1_manager = proxy;
      g_signal_connect (proxy, "g-signal",
                        G_CALLBACK(on_login1_manager_signal), self);
    }
}

/***
****  GDBus
***/

static void
on_bus_acquired (GDBusConnection * connection,
                 const gchar     * name,
                 gpointer          gself)
{
  int i;
  guint id;
  GError * err = NULL;
  IndicatorDatetimeService * self = INDICATOR_DATETIME_SERVICE(gself);
  priv_t * p = self->priv;

  g_debug ("bus acquired: %s", name);

  p->conn = g_object_ref (G_OBJECT (connection));

  /* export the actions */
  if ((id = g_dbus_connection_export_action_group (connection,
                                                   BUS_PATH,
                                                   G_ACTION_GROUP (p->actions),
                                                   &err)))
    {
      p->actions_export_id = id;
    }
  else
    {
      g_warning ("cannot export action group: %s", err->message);
      g_clear_error (&err);
    }

  /* export the menus */
  for (i=0; i<N_PROFILES; ++i)
    {
      char * path = g_strdup_printf ("%s/%s", BUS_PATH, menu_names[i]);
      struct ProfileMenuInfo * menu = &p->menus[i];

      if (menu->menu == NULL)
        create_menu (self, i);

      if ((id = g_dbus_connection_export_menu_model (connection,
                                                     path,
                                                     G_MENU_MODEL (menu->menu),
                                                     &err)))
        {
          menu->export_id = id;
        }
      else
        {
          g_warning ("cannot export %s menu: %s", menu_names[i], err->message);
          g_clear_error (&err);
        }

      g_free (path);
    }
}

static void
unexport (IndicatorDatetimeService * self)
{
  int i;
  priv_t * p = self->priv;

  /* unexport the menus */
  for (i=0; i<N_PROFILES; ++i)
    {
      guint * id = &self->priv->menus[i].export_id;

      if (*id)
        {
          g_dbus_connection_unexport_menu_model (p->conn, *id);
          *id = 0;
        }
    }

  /* unexport the actions */
  if (p->actions_export_id)
    {
      g_dbus_connection_unexport_action_group (p->conn, p->actions_export_id);
      p->actions_export_id = 0;
    }
}

static void
on_name_lost (GDBusConnection * connection G_GNUC_UNUSED,
              const gchar     * name,
              gpointer          gself)
{
  IndicatorDatetimeService * self = INDICATOR_DATETIME_SERVICE (gself);

  g_debug ("%s %s name lost %s", G_STRLOC, G_STRFUNC, name);

  unexport (self);

  g_signal_emit (self, signals[SIGNAL_NAME_LOST], 0, NULL);
}


/***
****  GObject virtual functions
***/

static void
my_dispose (GObject * o)
{
  int i;
  IndicatorDatetimeService * self = INDICATOR_DATETIME_SERVICE(o);
  priv_t * p = self->priv;

  if (p->own_id)
    {
      g_bus_unown_name (p->own_id);
      p->own_id = 0;
    }

  unexport (self);

  if (p->cancellable != NULL)
    {
      g_cancellable_cancel (p->cancellable);
      g_clear_object (&p->cancellable);
    }

  set_detect_location_enabled (self, FALSE);

  if (p->planner != NULL)
    {
      g_signal_handlers_disconnect_by_data (p->planner, self);
      g_clear_object (&p->planner);
    }

  if (p->login1_manager != NULL)
    {
      g_signal_handlers_disconnect_by_data (p->login1_manager, self);
      g_clear_object (&p->login1_manager);
    }

  indicator_clear_timer (&p->skew_timer);
  indicator_clear_timer (&p->rebuild_id);
  indicator_clear_timer (&p->timezone_timer);
  indicator_clear_timer (&p->header_timer);

  if (p->settings != NULL)
    {
      g_signal_handlers_disconnect_by_data (p->settings, self);
      g_clear_object (&p->settings);
    }

  g_clear_object (&p->actions);

  for (i=0; i<N_PROFILES; ++i)
    g_clear_object (&p->menus[i].menu);

  g_clear_object (&p->planner);
  g_clear_object (&p->calendar_action);
  g_clear_object (&p->header_action);
  g_clear_object (&p->conn);

  G_OBJECT_CLASS (indicator_datetime_service_parent_class)->dispose (o);
}

static void
my_finalize (GObject * o)
{
  IndicatorDatetimeService * self = INDICATOR_DATETIME_SERVICE(o);
  priv_t * p = self->priv;

  g_clear_pointer (&p->skew_time, g_date_time_unref);
  g_clear_pointer (&p->calendar_date, g_date_time_unref);

  G_OBJECT_CLASS (indicator_datetime_service_parent_class)->finalize (o);
}

/***
****  Instantiation
***/

static void
indicator_datetime_service_init (IndicatorDatetimeService * self)
{
  guint i, n;
  priv_t * p;
  GString * gstr = g_string_new (NULL);

  /* these are the settings that affect the
     contents of the respective sections */
  const char * const header_settings[] = {
    SETTINGS_SHOW_CLOCK_S,
    SETTINGS_TIME_FORMAT_S,
    SETTINGS_SHOW_SECONDS_S,
    SETTINGS_SHOW_DAY_S,
    SETTINGS_SHOW_DATE_S,
    SETTINGS_CUSTOM_TIME_FORMAT_S
  };
  const char * const calendar_settings[] = {
    SETTINGS_SHOW_CALENDAR_S,
    SETTINGS_SHOW_WEEK_NUMBERS_S
  };
  const char * const appointment_settings[] = {
    SETTINGS_SHOW_EVENTS_S,
    SETTINGS_TIME_FORMAT_S,
    SETTINGS_SHOW_SECONDS_S
  };
  const char * const location_settings[] = {
    SETTINGS_TIME_FORMAT_S,
    SETTINGS_SHOW_SECONDS_S,
    SETTINGS_CUSTOM_TIME_FORMAT_S,
    SETTINGS_SHOW_LOCATIONS_S,
    SETTINGS_LOCATIONS_S,
    SETTINGS_SHOW_DETECTED_S,
    SETTINGS_TIMEZONE_NAME_S
  };
  const char * const time_format_string_settings[] = {
    SETTINGS_TIME_FORMAT_S,
    SETTINGS_SHOW_SECONDS_S,
    SETTINGS_CUSTOM_TIME_FORMAT_S
  };
    

  /* init the priv pointer */

  p = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                   INDICATOR_TYPE_DATETIME_SERVICE,
                                   IndicatorDatetimeServicePrivate);
  self->priv = p;

  p->cancellable = g_cancellable_new ();

  /***
  ****  Create the planner and listen for changes
  ***/

  p->planner = indicator_datetime_planner_eds_new ();

  g_signal_connect_swapped (p->planner, "appointments-changed",
                            G_CALLBACK(rebuild_calendar_section_soon), self);


  /***
  ****  Create the settings object and listen for changes
  ***/

  p->settings = g_settings_new (SETTINGS_INTERFACE);
  for (i=0, n=G_N_ELEMENTS(header_settings); i<n; i++)
    {
      g_string_printf (gstr, "changed::%s", header_settings[i]);
      g_signal_connect_swapped (p->settings, gstr->str,
                                G_CALLBACK(rebuild_header_soon), self);
    }

  for (i=0, n=G_N_ELEMENTS(calendar_settings); i<n; i++)
    {
      g_string_printf (gstr, "changed::%s", calendar_settings[i]);
      g_signal_connect_swapped (p->settings, gstr->str,
                                G_CALLBACK(rebuild_calendar_section_soon), self);
    }

  for (i=0, n=G_N_ELEMENTS(appointment_settings); i<n; i++)
    {
      g_string_printf (gstr, "changed::%s", appointment_settings[i]);
      g_signal_connect_swapped (p->settings, gstr->str,
                                G_CALLBACK(rebuild_appointments_section_soon), self);
    }

  for (i=0, n=G_N_ELEMENTS(location_settings); i<n; i++)
    {
      g_string_printf (gstr, "changed::%s", location_settings[i]);
      g_signal_connect_swapped (p->settings, gstr->str,
                                G_CALLBACK(rebuild_locations_section_soon), self);
    }

  /* The keys in time_format_string_settings affect the time format strings we build.
     When these change, we need to rebuild everything that has a time format string. */
  for (i=0, n=G_N_ELEMENTS(time_format_string_settings); i<n; i++)
    {
      g_string_printf (gstr, "changed::%s", time_format_string_settings[i]);
      g_signal_connect_swapped (p->settings, gstr->str,
                                G_CALLBACK(on_local_time_jumped), self);
    }

  init_gactions (self);

  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                            G_DBUS_PROXY_FLAGS_NONE,
                            NULL,
                            "org.freedesktop.login1",
                            "/org/freedesktop/login1",
                            "org.freedesktop.login1.Manager",
                            p->cancellable,
                            on_login1_manager_proxy_ready,
                            self);

  p->skew_timer = g_timeout_add_seconds (SKEW_CHECK_INTERVAL_SEC,
                                         skew_timer_func,
                                         self);

  p->own_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                              BUS_NAME,
                              G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT,
                              on_bus_acquired,
                              NULL,
                              on_name_lost,
                              self,
                              NULL);

  on_local_time_jumped (self);

  g_string_free (gstr, TRUE);
}

static void
indicator_datetime_service_class_init (IndicatorDatetimeServiceClass * klass)
{
  GObjectClass * object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = my_dispose;
  object_class->finalize = my_finalize;

  g_type_class_add_private (klass, sizeof (IndicatorDatetimeServicePrivate));

  signals[SIGNAL_NAME_LOST] = g_signal_new (
    INDICATOR_DATETIME_SERVICE_SIGNAL_NAME_LOST,
    G_TYPE_FROM_CLASS(klass),
    G_SIGNAL_RUN_LAST,
    G_STRUCT_OFFSET (IndicatorDatetimeServiceClass, name_lost),
    NULL, NULL,
    g_cclosure_marshal_VOID__VOID,
    G_TYPE_NONE, 0);
}

/***
****  Public API
***/

IndicatorDatetimeService *
indicator_datetime_service_new (void)
{
  GObject * o = g_object_new (INDICATOR_TYPE_DATETIME_SERVICE, NULL);

  return INDICATOR_DATETIME_SERVICE (o);
}

/* This currently just returns the system time,
   As we add test coverage, we'll need this to bypass the system time. */
GDateTime *
indicator_datetime_service_get_localtime (IndicatorDatetimeService * self G_GNUC_UNUSED)
{
  return g_date_time_new_now_local ();
}

void
indicator_datetime_service_set_calendar_date (IndicatorDatetimeService * self,
                                              GDateTime                * date)
{
  gboolean dirty;
  priv_t * p = self->priv;

  dirty = !date || !p->calendar_date || g_date_time_compare (date, p->calendar_date);

  /* update calendar_date */
  g_clear_pointer (&p->calendar_date, g_date_time_unref);
  if (date != NULL)
    p->calendar_date = g_date_time_ref (date);

  /* sync the menuitems and action states */
  if (dirty)
    {
      update_calendar_action_state (self);
      rebuild_appointments_section_soon (self);
    }
}
