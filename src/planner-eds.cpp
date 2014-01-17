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

G_DEFINE_QUARK("source-client", source_client)


class PlannerEds::Impl
{
public:

    Impl(PlannerEds& owner):
        m_owner(owner),
        m_cancellable(g_cancellable_new())
    {
        e_source_registry_new(m_cancellable, on_source_registry_ready, this);

        m_owner.time.changed().connect([this](const DateTime& dt) {
            g_message("planner's datetime property changed to %s; calling rebuildSoon()", g_date_time_format(dt.get(), "%F %T"));
            rebuildSoon();
        });

        rebuildSoon();
    }

    ~Impl()
    {
        g_cancellable_cancel(m_cancellable);
        g_clear_object(&m_cancellable);

        if (m_rebuild_tag)
            g_source_remove(m_rebuild_tag);

        if (m_source_registry)
            g_signal_handlers_disconnect_by_data(m_source_registry, this);
        g_clear_object(&m_source_registry);
    }

private:

    static void on_source_registry_ready(GObject* /*source*/, GAsyncResult* res, gpointer gself)
    {
        GError * error = nullptr;
        auto r = e_source_registry_new_finish(res, &error);
        if (error != nullptr)
        {
            if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                g_warning("indicator-datetime cannot show EDS appointments: %s", error->message);

            g_error_free(error);
        }
        else
        {
            auto self = static_cast<Impl*>(gself);

            g_signal_connect(r, "source-added",    G_CALLBACK(on_source_added), self);
            g_signal_connect(r, "source-removed",  G_CALLBACK(on_source_removed), self);
            g_signal_connect(r, "source-changed",  G_CALLBACK(on_source_changed), self);
            g_signal_connect(r, "source-disabled", G_CALLBACK(on_source_disabled), self);
            g_signal_connect(r, "source-enabled",  G_CALLBACK(on_source_enabled), self);

            self->m_source_registry = r;

            GList* sources = e_source_registry_list_sources(r, E_SOURCE_EXTENSION_CALENDAR);
            for (auto l=sources; l!=nullptr; l=l->next)
                on_source_added(r, E_SOURCE(l->data), gself);
            g_list_free_full(sources, g_object_unref);
        }
    }

    static void on_source_added(ESourceRegistry* registry, ESource* source, gpointer gself)
    {
        auto self = static_cast<PlannerEds::Impl*>(gself);

        self->m_sources.insert(E_SOURCE(g_object_ref(source)));

        if (e_source_get_enabled(source))
            on_source_enabled(registry, source, gself);
    }

    static void on_source_enabled(ESourceRegistry* /*registry*/, ESource* source, gpointer gself)
    {
        auto self = static_cast<PlannerEds::Impl*>(gself);

        e_cal_client_connect(source,
                             E_CAL_CLIENT_SOURCE_TYPE_EVENTS,
                             self->m_cancellable,
                             on_client_connected,
                             gself);
    }

    static void on_client_connected(GObject* /*source*/, GAsyncResult * res, gpointer gself)
    {
        GError * error = nullptr;
        EClient * client = e_cal_client_connect_finish(res, &error);
        if (error)
        {
            if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                g_warning("indicator-datetime cannot connect to EDS source: %s", error->message);

            g_error_free(error);
        }
        else
        {
            // we've got a new connected ECalClient, so store it & notify clients
            g_object_set_qdata_full(G_OBJECT(e_client_get_source(client)),
                                    source_client_quark(),
                                    client,
                                    g_object_unref);

            g_message("client connected; calling rebuildSoon()");
            static_cast<Impl*>(gself)->rebuildSoon();
        }
    }

    static void on_source_disabled(ESourceRegistry* /*registry*/, ESource* source, gpointer gself)
    {
        gpointer e_cal_client;

        // if this source has a connected ECalClient, remove it & notify clients
        if ((e_cal_client = g_object_steal_qdata(G_OBJECT(source), source_client_quark())))
        {
            g_object_unref(e_cal_client);

            g_message("source disabled; calling rebuildSoon()");
            static_cast<Impl*>(gself)->rebuildSoon();
        }
    }

    static void on_source_removed(ESourceRegistry* registry, ESource* source, gpointer gself)
    {
        auto self = static_cast<PlannerEds::Impl*>(gself);

        on_source_disabled(registry, source, gself);

        self->m_sources.erase(source);
        g_object_unref(source);
    }

    static void on_source_changed(ESourceRegistry* /*registry*/, ESource* /*source*/, gpointer gself)
    {
        g_message("source changed; calling rebuildSoon()");
        static_cast<Impl*>(gself)->rebuildSoon();
    }

private:

    typedef std::function<void(const std::vector<Appointment>&)> appointment_func;

    struct Task
    {
        Impl* p;
        appointment_func func;
        std::vector<Appointment> appointments;
        Task(Impl* p_in, const appointment_func& func_in): p(p_in), func(func_in) {}
    };

    struct AppointmentSubtask
    {
        std::shared_ptr<Task> task;
        ECalClient* client;
        std::string color;
        AppointmentSubtask(const std::shared_ptr<Task>& task_in, ECalClient* client_in, const char* color_in):
            task(task_in), client(client_in), color(color_in) {}
    };

    void rebuildSoon()
    {
        const static guint ARBITRARY_INTERVAL_SECS = 2;

        if (m_rebuild_tag == 0)
            m_rebuild_tag = g_timeout_add_seconds(ARBITRARY_INTERVAL_SECS, rebuildNowStatic, this);
    }

    static gboolean rebuildNowStatic(gpointer gself)
    {
        auto self = static_cast<Impl*>(gself);
        self->m_rebuild_tag = 0;
        self->rebuildNow();
        return G_SOURCE_REMOVE;
    }

    void rebuildNow()
    {
        auto calendar_date = m_owner.time.get().get();
        GDateTime* begin;
        GDateTime* end;
        int y, m, d;
        g_message("in rebuildNow");

        // get all the appointments in the calendar month
        g_date_time_get_ymd(calendar_date, &y, &m, &d);
        begin = g_date_time_new_local(y, m, 1, 0, 0, 0.1);
        end = g_date_time_new_local(y, m, g_date_get_days_in_month(GDateMonth(m),GDateYear(y)), 23, 59, 59.9);
        if (begin && end)
        {
            getAppointments(begin, end, [this](const std::vector<Appointment>& appointments) {
                g_message("got %d appointments in this calendar month", (int)appointments.size());
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
                g_message("got %d upcoming appointments", (int)appointments.size());
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
        g_message("getting all appointments from [%s ... %s]", g_date_time_format(begin_dt, "%F %T"),
                                                                g_date_time_format(end_dt, "%F %T"));

        /**
        ***  init the default timezone
        **/

        icaltimezone * default_timezone = nullptr;

        const auto tz = g_date_time_get_timezone_abbreviation(m_owner.time.get().get());
        g_message("%s tz is %s", G_STRLOC, tz);
        if (tz && *tz)
        {
            default_timezone = icaltimezone_get_builtin_timezone(tz);

            if (default_timezone == nullptr) // maybe str is a tzid?
                default_timezone = icaltimezone_get_builtin_timezone_from_tzid(tz);

            g_debug("default_timezone is %p", (void*)default_timezone);
        }

        /**
        ***  walk through the sources to build the appointment list
        **/

        std::shared_ptr<Task> main_task(new Task(this, func), [](Task* task){
            g_message("time to delete task %p", (void*)task);
            task->func(task->appointments);
        });

        for (auto& source : m_sources)
        {
            auto client = E_CAL_CLIENT(g_object_get_qdata(G_OBJECT(source), source_client_quark()));
            if (client == nullptr)
                continue;

            if (default_timezone != nullptr)
                e_cal_client_set_default_timezone(client, default_timezone);

            // start a new subtask to enumerate all the components in this client.
            auto extension = e_source_get_extension(source, E_SOURCE_EXTENSION_CALENDAR);
            const auto color = e_source_selectable_get_color(E_SOURCE_SELECTABLE(extension));
            g_message("calling e_cal_client_generate_instances for %p", (void*)client);
            e_cal_client_generate_instances(client,
                                            begin,
                                            end,
                                            m_cancellable,
                                            my_get_appointments_foreach,
                                            new AppointmentSubtask (main_task, client, color),
                                            [](gpointer g){delete static_cast<AppointmentSubtask*>(g);});
        }
    }

    struct UrlSubtask
    {
        std::shared_ptr<Task> task;
        Appointment appointment;
        UrlSubtask(const std::shared_ptr<Task>& task_in, const Appointment& appointment_in):
            task(task_in), appointment(appointment_in) {}
    };

    static gboolean
    my_get_appointments_foreach(ECalComponent* component,
                                time_t         begin,
                                time_t         end,
                                gpointer       gsubtask)
    {
        const auto vtype = e_cal_component_get_vtype(component);
        auto subtask = static_cast<AppointmentSubtask*>(gsubtask);

        if ((vtype == E_CAL_COMPONENT_EVENT) || (vtype == E_CAL_COMPONENT_TODO))
        {
          const gchar* uid = nullptr;
          e_cal_component_get_uid(component, &uid);

          auto status = ICAL_STATUS_NONE;
          e_cal_component_get_status(component, &status);

          if ((uid != nullptr) &&
              (status != ICAL_STATUS_COMPLETED) &&
              (status != ICAL_STATUS_CANCELLED))
          {
              Appointment appointment;

              /* Determine whether this is a recurring event.
                 NB: icalrecurrencetype supports complex recurrence patterns;
                 however, since design only allows daily recurrence,
                 that's all we support here. */
              GSList * recur_list;
              e_cal_component_get_rrule_list(component, &recur_list);
              for (auto l=recur_list; l!=nullptr; l=l->next)
              {
                  const auto recur = static_cast<struct icalrecurrencetype*>(l->data);
                  appointment.is_daily |= ((recur->freq == ICAL_DAILY_RECURRENCE)
                                             && (recur->interval == 1));
              }
              e_cal_component_free_recur_list(recur_list);

              ECalComponentText text;
              text.value = "";
              e_cal_component_get_summary(component, &text);
 
              appointment.begin = g_date_time_new_from_unix_local(begin);
              appointment.end = g_date_time_new_from_unix_local(end);
              appointment.color = subtask->color;
              appointment.is_event = vtype == E_CAL_COMPONENT_EVENT;
              appointment.summary = text.value;
              appointment.uid = uid;

              GList * alarm_uids = e_cal_component_get_alarm_uids(component);
              appointment.has_alarms = alarm_uids != nullptr;
              cal_obj_uid_list_free(alarm_uids);

              e_cal_client_get_attachment_uris(subtask->client,
                                               uid,
                                               nullptr,
                                               subtask->task->p->m_cancellable,
                                               on_appointment_uris_ready,
                                               new UrlSubtask(subtask->task, appointment));
            }
        }

        return G_SOURCE_CONTINUE;
    }

    static void on_appointment_uris_ready(GObject* client, GAsyncResult* res, gpointer gsubtask)
    {
        auto subtask = static_cast<UrlSubtask*>(gsubtask);

        GSList * uris = nullptr;
        GError * error = nullptr;
        e_cal_client_get_attachment_uris_finish(E_CAL_CLIENT(client), res, &uris, &error);
        if (error != nullptr)
        {
            if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                g_warning("Error getting appointment uris: %s", error->message);

            g_error_free(error);
        }
        else if (uris != nullptr)
        {
            subtask->appointment.url = (const char*) uris->data; // copy the first URL
            g_debug("found url '%s' for appointment '%s'", subtask->appointment.url.c_str(), subtask->appointment.summary.c_str());
            e_client_util_free_string_slist(uris);
        }

        g_message("adding appointment '%s' '%s'", subtask->appointment.summary.c_str(), subtask->appointment.url.c_str());
        subtask->task->appointments.push_back(subtask->appointment);
        delete subtask;
    }

private:

    PlannerEds& m_owner;
    std::set<ESource*> m_sources;
    GCancellable * m_cancellable = nullptr;
    ESourceRegistry * m_source_registry = nullptr;
    guint m_rebuild_tag = 0;
};

PlannerEds::PlannerEds(): p(new Impl(*this)) {}

PlannerEds::~PlannerEds() =default;

} // namespace datetime
} // namespace indicator
} // namespace unity
