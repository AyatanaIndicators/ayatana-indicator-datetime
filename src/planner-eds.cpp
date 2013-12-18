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

#include <datetime/planner-eds.h>

#include <datetime/appointment.h>

#include <libical/ical.h>
#include <libical/icaltime.h>
#include <libecal/libecal.h>
#include <libedataserver/libedataserver.h>

namespace unity {
namespace indicator {
namespace datetime {

/****
*****
****/

G_DEFINE_QUARK ("source-client", source_client)


class PlannerEds::Impl
{
public:

    Impl (PlannerEds& owner):
        owner_(owner),
        cancellable_(g_cancellable_new())
    {
        e_source_registry_new (cancellable_, on_source_registry_ready, this);

        owner_.time.changed().connect([this](const DateTime& dt) {
            g_message ("planner's datetime property changed to %s; calling rebuildSoon()", g_date_time_format(dt.get(), "%F %T"));
            rebuildSoon();
        });

        rebuildSoon();
    }

    ~Impl()
    {
        g_cancellable_cancel (cancellable_);
        g_clear_object (&cancellable_);

        if (rebuild_tag_)
            g_source_remove (rebuild_tag_);

        if (source_registry_)
            g_signal_handlers_disconnect_by_data (source_registry_, this);
        g_clear_object (&source_registry_);
    }

private:

    static void on_source_registry_ready (GObject* /*source*/, GAsyncResult* res, gpointer gself)
    {
        GError * error = NULL;
        auto r = e_source_registry_new_finish (res, &error);
        if (error != NULL)
        {
            if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                g_warning ("indicator-datetime cannot show EDS appointments: %s", error->message);

            g_error_free (error);
        }
        else
        {
            auto self = static_cast<Impl*>(gself);

            g_signal_connect (r, "source-added",    G_CALLBACK(on_source_added), self);
            g_signal_connect (r, "source-removed",  G_CALLBACK(on_source_removed), self);
            g_signal_connect (r, "source-changed",  G_CALLBACK(on_source_changed), self);
            g_signal_connect (r, "source-disabled", G_CALLBACK(on_source_disabled), self);
            g_signal_connect (r, "source-enabled",  G_CALLBACK(on_source_enabled), self);

            self->source_registry_ = r;

            GList* sources = e_source_registry_list_sources (r, E_SOURCE_EXTENSION_CALENDAR);
            for (auto l=sources; l!=nullptr; l=l->next)
                on_source_added (r, E_SOURCE(l->data), gself);
            g_list_free_full (sources, g_object_unref);
        }
    }

    static void on_source_added(ESourceRegistry* registry, ESource* source, gpointer gself)
    {
        auto self = static_cast<PlannerEds::Impl*>(gself);

        self->sources_.insert(E_SOURCE(g_object_ref(source)));

        if (e_source_get_enabled(source))
            on_source_enabled(registry, source, gself);
    }

    static void on_source_enabled (ESourceRegistry* /*registry*/, ESource* source, gpointer gself)
    {
        auto self = static_cast<PlannerEds::Impl*>(gself);

        e_cal_client_connect (source,
                              E_CAL_CLIENT_SOURCE_TYPE_EVENTS,
                              self->cancellable_,
                              on_client_connected,
                              gself);
    }

    static void on_client_connected (GObject* /*source*/, GAsyncResult * res, gpointer gself)
    {
        GError * error = nullptr;
        EClient * client = e_cal_client_connect_finish (res, &error);
        if (error)
        {
            if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                g_warning ("indicator-datetime cannot connect to EDS source: %s", error->message);

            g_error_free (error);
        }
        else
        {
            // we've got a new connected ECalClient, so store it & notify clients
            g_object_set_qdata_full (G_OBJECT(e_client_get_source(client)),
                                     source_client_quark(),
                                     client,
                                     g_object_unref);

            g_message ("client connected; calling rebuildSoon()");
            static_cast<Impl*>(gself)->rebuildSoon();
        }
    }

    static void on_source_disabled (ESourceRegistry* /*registry*/, ESource* source, gpointer gself)
    {
        gpointer e_cal_client;

        // if this source has a connected ECalClient, remove it & notify clients
        if ((e_cal_client = g_object_steal_qdata (G_OBJECT(source), source_client_quark())))
        {
            g_object_unref (e_cal_client);

            g_message ("source disabled; calling rebuildSoon()");
            static_cast<Impl*>(gself)->rebuildSoon();
        }
    }

    static void on_source_removed (ESourceRegistry* registry, ESource* source, gpointer gself)
    {
        auto self = static_cast<PlannerEds::Impl*>(gself);

        on_source_disabled (registry, source, gself);

        self->sources_.erase (source);
        g_object_unref (source);
    }

    static void on_source_changed (ESourceRegistry* /*registry*/, ESource* /*source*/, gpointer gself)
    {
        g_message ("source changed; calling rebuildSoon()");
        static_cast<Impl*>(gself)->rebuildSoon();
    }

private:

    typedef std::function<void(const std::vector<Appointment>&)> appointment_func;

    struct Task
    {
        Impl* impl_;
        appointment_func func_;
        std::vector<Appointment> appointments_;
        Task (Impl* impl, const appointment_func& func): impl_(impl), func_(func) {}
    };

    struct AppointmentSubtask
    {
        std::shared_ptr<Task> task_;
        ECalClient * client_;
        std::string color_;
        AppointmentSubtask (const std::shared_ptr<Task>& task, ECalClient* client, const char* color):
                task_(task), client_(client), color_(color) {}
    };

    void rebuildSoon()
    {
        const static guint ARBITRARY_INTERVAL_SECS = 2;

        if (rebuild_tag_ == 0)
            rebuild_tag_ = g_timeout_add_seconds (ARBITRARY_INTERVAL_SECS, rebuildNowStatic, this);
    }

    static gboolean rebuildNowStatic (gpointer gself)
    {
        auto self = static_cast<Impl*>(gself);
        self->rebuild_tag_ = 0;
        self->rebuildNow();
        return G_SOURCE_REMOVE;
    }

    void rebuildNow()
    {
        GDateTime* calendar_date = owner_.time.get().get();
        GDateTime* begin;
        GDateTime* end;
        int y, m, d;
        g_message ("in rebuildNow");

        // get all the appointments in the calendar month
        g_date_time_get_ymd(calendar_date, &y, &m, &d);
        begin = g_date_time_new_local(y, m, 1, 0, 0, 0.1);
        end = g_date_time_new_local(y, m, g_date_get_days_in_month(GDateMonth(m),GDateYear(y)), 23, 59, 59.9);
        if (begin && end)
        {
            getAppointments(begin, end, [this](const std::vector<Appointment>& appointments) {
                g_message ("got %d appointments in this calendar month", (int)appointments.size());
            });
        }
        g_clear_pointer(&begin, g_date_time_unref);
        g_clear_pointer(&end, g_date_time_unref);

        // get the upcoming appointments
        begin = g_date_time_ref(calendar_date);
        end = g_date_time_add_months(begin, 1);
        if (begin && end)
        {
            getAppointments(begin, end, [this](const std::vector<Appointment>& appointments) {
                g_message ("got %d upcoming appointments", (int)appointments.size());
            });
        }
        g_clear_pointer(&begin, g_date_time_unref);
        g_clear_pointer(&end, g_date_time_unref);
        g_clear_pointer(&calendar_date, g_date_time_unref);
    }

    void getAppointments(GDateTime* begin_dt, GDateTime* end_dt, appointment_func func)
    {
        const auto begin = g_date_time_to_unix(begin_dt);
        const auto end = g_date_time_to_unix(end_dt);
        g_message ("getting all appointments from [%s ... %s]", g_date_time_format (begin_dt, "%F %T"),
                                                                g_date_time_format (end_dt, "%F %T"));

        /**
        ***  init the default timezone
        **/

        icaltimezone * default_timezone = nullptr;

        const auto tz = g_date_time_get_timezone_abbreviation (owner_.time.get().get());
        g_message ("%s tz is %s", G_STRLOC, tz);
        if (tz && *tz)
        {
            default_timezone = icaltimezone_get_builtin_timezone (tz);

            if (default_timezone == nullptr) // maybe str is a tzid?
                default_timezone = icaltimezone_get_builtin_timezone_from_tzid (tz);

            g_debug ("default_timezone is %p", default_timezone);
        }

        /**
        ***  walk through the sources to build the appointment list
        **/

        std::shared_ptr<Task> main_task (new Task(this, func), [](Task* task){
            g_message("time to delete task %p", task);
            task->func_(task->appointments_);
        });

        for (auto& source : sources_)
        {
            auto client = E_CAL_CLIENT (g_object_get_qdata (G_OBJECT(source), source_client_quark()));
            if (client == nullptr)
                continue;

            if (default_timezone != nullptr)
                e_cal_client_set_default_timezone (client, default_timezone);

            // start a new subtask to enumerate all the components in this client.
            auto extension = e_source_get_extension (source, E_SOURCE_EXTENSION_CALENDAR);
            const auto color = e_source_selectable_get_color (E_SOURCE_SELECTABLE(extension));
            g_message ("calling e_cal_client_generate_instances for %p", client);
            e_cal_client_generate_instances (client,
                                             begin,
                                             end,
                                             cancellable_,
                                             my_get_appointments_foreach,
                                             new AppointmentSubtask (main_task, client, color),
                                             [](gpointer g){delete static_cast<AppointmentSubtask*>(g);});
        }
    }

    struct UrlSubtask
    {
        std::shared_ptr<Task> task_;
        Appointment appointment_;
        UrlSubtask (const std::shared_ptr<Task>& task, const Appointment& appointment): task_(task), appointment_(appointment) {}
    };

    static gboolean
    my_get_appointments_foreach (ECalComponent* component,
                                 time_t         begin,
                                 time_t         end,
                                 gpointer       gsubtask)
    {
        const auto vtype = e_cal_component_get_vtype(component);
        auto subtask = static_cast<AppointmentSubtask*>(gsubtask);

        if ((vtype == E_CAL_COMPONENT_EVENT) || (vtype == E_CAL_COMPONENT_TODO))
        {
          const gchar* uid = NULL;
          e_cal_component_get_uid (component, &uid);

          auto status = ICAL_STATUS_NONE;
          e_cal_component_get_status (component, &status);

          if ((uid != NULL) &&
              (status != ICAL_STATUS_COMPLETED) &&
              (status != ICAL_STATUS_CANCELLED))
          {
              Appointment appointment;

              /* Determine whether this is a recurring event.
                 NB: icalrecurrencetype supports complex recurrence patterns;
                 however, since design only allows daily recurrence,
                 that's all we support here. */
              GSList * recur_list;
              e_cal_component_get_rrule_list (component, &recur_list);
              for (auto l=recur_list; l!=NULL; l=l->next)
              {
                  const auto recur = static_cast<struct icalrecurrencetype*>(l->data);
                  appointment.is_daily |= ((recur->freq == ICAL_DAILY_RECURRENCE)
                                             && (recur->interval == 1));
              }
              e_cal_component_free_recur_list (recur_list);

              ECalComponentText text;
              text.value = "";
              e_cal_component_get_summary (component, &text);
 
              appointment.begin = g_date_time_new_from_unix_local (begin);
              appointment.end = g_date_time_new_from_unix_local (end);
              appointment.color = subtask->color_;
              appointment.is_event = vtype == E_CAL_COMPONENT_EVENT;
              appointment.summary = text.value;
              appointment.uid = uid;

              GList * alarm_uids = e_cal_component_get_alarm_uids (component);
              appointment.has_alarms = alarm_uids != nullptr;
              cal_obj_uid_list_free (alarm_uids);

              e_cal_client_get_attachment_uris (subtask->client_,
                                                uid,
                                                NULL,
                                                subtask->task_->impl_->cancellable_,
                                                on_appointment_uris_ready,
                                                new UrlSubtask(subtask->task_, appointment));
            }
        }

        return G_SOURCE_CONTINUE;
    }

    static void on_appointment_uris_ready (GObject* client, GAsyncResult* res, gpointer gsubtask)
    {
        auto subtask = static_cast<UrlSubtask*>(gsubtask);

        GSList * uris = nullptr;
        GError * error = nullptr;
        e_cal_client_get_attachment_uris_finish (E_CAL_CLIENT(client), res, &uris, &error);
        if (error != NULL)
        {
            if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                g_warning ("Error getting appointment uris: %s", error->message);

            g_error_free (error);
        }
        else if (uris != NULL)
        {
            subtask->appointment_.url = (const char*) uris->data; // copy the first URL
            g_debug ("found url '%s' for appointment '%s'", subtask->appointment_.url.c_str(), subtask->appointment_.summary.c_str());
            e_client_util_free_string_slist (uris);
        }

        g_message ("adding appointment '%s' '%s'", subtask->appointment_.summary.c_str(), subtask->appointment_.url.c_str());
        subtask->task_->appointments_.push_back (subtask->appointment_);
        delete subtask;
    }

private:

    PlannerEds& owner_;
    std::set<ESource*> sources_;
    GCancellable * cancellable_ = nullptr;
    ESourceRegistry * source_registry_ = nullptr;
    guint rebuild_tag_ = 0;
};

PlannerEds::PlannerEds(): impl_(new Impl(*this)) {}

PlannerEds::~PlannerEds() =default;

} // namespace datetime
} // namespace indicator
} // namespace unity
