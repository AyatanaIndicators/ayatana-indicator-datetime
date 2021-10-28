/*
 * Copyright 2013 Canonical Ltd.
 * Copyright 2021 Robert Tari
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
 *   Robert Tari <robert@tari.in>
 */

#include <datetime/dbus-shared.h>
#include <datetime/actions-live.h>

#include <glib.h>

#include <sstream>

extern "C"
{
    #include <ayatana/common/utils.h>
}

namespace ayatana {
namespace indicator {
namespace datetime {

/***
****
***/

LiveActions::LiveActions(const std::shared_ptr<State>& state_in):
    Actions(state_in)
{
}

/***
****
***/

std::string LiveActions::open_alarm_app()
{
    std::string sReturn = "";

    if (ayatana_common_utils_is_lomiri())
    {
        sReturn = "alarm://";
        ayatana_common_utils_open_url(sReturn.c_str());
    }
    else
    {
        sReturn = "evolution -c calendar";
        ayatana_common_utils_execute_command(sReturn.c_str());
    }

    return sReturn;
}

std::string LiveActions::open_calendar_app(const DateTime& dt)
{
    std::string sReturn = "";

    if (ayatana_common_utils_is_lomiri())
    {
        const auto utc = dt.to_timezone("UTC");
        sReturn = utc.format("calendar://startdate=%Y-%m-%dT%H:%M:%S+00:00");
        ayatana_common_utils_open_url(sReturn.c_str());
    }
    else
    {
        const auto utc = dt.start_of_day().to_timezone("UTC");
        sReturn = utc.format("evolution \"calendar:///?startdate=%Y%m%dT%H%M%SZ\"");
        ayatana_common_utils_execute_command(sReturn.c_str());
    }

    return sReturn;
}

std::string LiveActions::open_settings_app()
{
    std::string sReturn = "";

    if (ayatana_common_utils_is_lomiri())
    {
        sReturn = "settings:///system/time-date";
        ayatana_common_utils_open_url(sReturn.c_str());
    }
    else if (ayatana_common_utils_is_unity())
    {
        sReturn = "unity-control-center datetime";
        ayatana_common_utils_execute_command(sReturn.c_str());
    }
    else if (ayatana_common_utils_is_mate())
    {
        sReturn = "mate-time-admin";
        ayatana_common_utils_execute_command(sReturn.c_str());
    }
    else
    {
        sReturn = "gnome-control-center datetime";
        ayatana_common_utils_execute_command(sReturn.c_str());
    }

    return sReturn;
}

bool LiveActions::desktop_has_calendar_app() const
{
    static bool inited = false;
    static bool have_calendar = false;

    if (G_UNLIKELY(!inited))
    {
        inited = true;

#if 0
        auto all = g_app_info_get_all_for_type ("text/calendar");
        for(auto l=all; !have_calendar && l!=nullptr; l=l->next)
        {
            auto app_info = static_cast<GAppInfo*>(l->data);

            if (!g_strcmp0("evolution.desktop", g_app_info_get_id(app_info)))
                have_calendar = true;
        }

        g_list_free_full(all, (GDestroyNotify)g_object_unref);
#else
        /* Work around http://pad.lv/1296233 for Trusty...
           let's revert this when the GIO bug is fixed. */
        auto executable = g_find_program_in_path("evolution");
        have_calendar = executable != nullptr;
        g_free(executable);
#endif
    }

    return have_calendar;
}

std::string LiveActions::open_appointment(const Appointment& appt, const DateTime& date)
{
    std::string sReturn = "";

    switch (appt.type)
    {
        case Appointment::ALARM:
            sReturn = open_alarm_app();
            break;

        case Appointment::EVENT:
        default:
            sReturn = open_calendar_app(date);
            break;
    }

    return sReturn;
}

/***
****
***/

namespace
{

struct setlocation_data
{
  std::string tzid;
  std::string name;
  std::shared_ptr<Settings> settings;
};

static void
on_datetime1_set_timezone_response(GObject       * object,
                                   GAsyncResult  * res,
                                   gpointer        gdata)
{
  GError* err = nullptr;
  auto response = g_dbus_proxy_call_finish(G_DBUS_PROXY(object), res, &err);
  auto data = static_cast<struct setlocation_data*>(gdata);

  if (err != nullptr)
    {
      if (!g_error_matches(err, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning("Could not set new timezone: %s", err->message);

      g_error_free(err);
    }
  else
    {
      data->settings->timezone_name.set(data->tzid + " " + data->name);
      g_variant_unref(response);
    }

  delete data;
}

static void
on_datetime1_proxy_ready (GObject      * object G_GNUC_UNUSED,
                          GAsyncResult * res,
                          gpointer       gdata)
{
  auto data = static_cast<struct setlocation_data*>(gdata);

  GError * err = nullptr;
  auto proxy = g_dbus_proxy_new_for_bus_finish(res, &err);
  if (err != nullptr)
    {
      if (!g_error_matches(err, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning("Could not grab DBus proxy for timedated: %s", err->message);

      g_error_free(err);

      delete data;
    }
  else
    {
      g_dbus_proxy_call(proxy,
                        Bus::Timedate1::Methods::SET_TIMEZONE,
                        g_variant_new ("(sb)", data->tzid.c_str(), TRUE),
                        G_DBUS_CALL_FLAGS_NONE,
                        -1,
                        nullptr,
                        on_datetime1_set_timezone_response,
                        data);

      g_object_unref (proxy);
    }
}

} // unnamed namespace


void LiveActions::set_location(const std::string& tzid, const std::string& name)
{
    g_return_if_fail(!tzid.empty());
    g_return_if_fail(!name.empty());

    auto data = new struct setlocation_data;
    data->tzid = tzid;
    data->name = name;
    data->settings = state()->settings;

    g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                              G_DBUS_PROXY_FLAGS_NONE,
                              nullptr,
                              Bus::Timedate1::BUSNAME,
                              Bus::Timedate1::ADDR,
                              Bus::Timedate1::IFACE,
                              nullptr,
                              on_datetime1_proxy_ready,
                              data);
}

/***
****
***/

} // namespace datetime
} // namespace indicator
} // namespace ayatana
