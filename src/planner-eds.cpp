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

#include <map>
#include <set>

namespace unity {
namespace indicator {
namespace datetime {

/****
*****
****/

class PlannerEds::Impl
{
public:

    Impl(PlannerEds& owner):
        m_owner(owner),
        m_cancellable(g_cancellable_new())
    {
        e_source_registry_new(m_cancellable, on_source_registry_ready, this);

        m_owner.time.changed().connect([this](const DateTime& dt) {
            g_debug("planner's datetime property changed to %s; calling rebuild_soon()", dt.format("%F %T").c_str());
            rebuild_soon();
        });

        rebuild_soon();
    }

    ~Impl()
    {
        g_cancellable_cancel(m_cancellable);
        g_clear_object(&m_cancellable);

        while(!m_sources.empty())
            remove_source(*m_sources.begin());

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
            g_signal_connect(r, "source-added",    G_CALLBACK(on_source_added),    gself);
            g_signal_connect(r, "source-removed",  G_CALLBACK(on_source_removed),  gself);
            g_signal_connect(r, "source-changed",  G_CALLBACK(on_source_changed),  gself);
            g_signal_connect(r, "source-disabled", G_CALLBACK(on_source_disabled), gself);
            g_signal_connect(r, "source-enabled",  G_CALLBACK(on_source_enabled),  gself);

            auto self = static_cast<Impl*>(gself);
            self->m_source_registry = r;
            self->add_sources_by_extension(E_SOURCE_EXTENSION_CALENDAR);
            self->add_sources_by_extension(E_SOURCE_EXTENSION_TASK_LIST);
        }
    }

    void add_sources_by_extension(const char* extension)
    {
        auto& r = m_source_registry;
        auto sources = e_source_registry_list_sources(r, extension);
        for (auto l=sources; l!=nullptr; l=l->next)
            on_source_added(r, E_SOURCE(l->data), this);
        g_list_free_full(sources, g_object_unref);
    }

    static void on_source_added(ESourceRegistry* registry, ESource* source, gpointer gself)
    {
        auto self = static_cast<Impl*>(gself);

        self->m_sources.insert(E_SOURCE(g_object_ref(source)));

        if (e_source_get_enabled(source))
            on_source_enabled(registry, source, gself);
    }

    static void on_source_enabled(ESourceRegistry* /*registry*/, ESource* source, gpointer gself)
    {
        auto self = static_cast<Impl*>(gself);

        g_debug("connecting a client to source %s", e_source_get_uid(source));
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
            // add the client to our collection
            auto self = static_cast<Impl*>(gself);
            g_debug("got a client for %s", e_cal_client_get_local_attachment_store(E_CAL_CLIENT(client)));
            self->m_clients[e_client_get_source(client)] = E_CAL_CLIENT(client);

            // now create a view for it so that we can listen for changes
            e_cal_client_get_view (E_CAL_CLIENT(client),
                                   "#t", // match all
                                   self->m_cancellable,
                                   on_client_view_ready,
                                   self);

            g_debug("client connected; calling rebuild_soon()");
            self->rebuild_soon();
        }
    }

    static void on_client_view_ready (GObject* client, GAsyncResult* res, gpointer gself)
    {
        GError* error = nullptr;
        ECalClientView* view = nullptr;

        if (e_cal_client_get_view_finish (E_CAL_CLIENT(client), res, &view, &error))
        {
            // add the view to our collection
            e_cal_client_view_start(view, &error);
            g_debug("got a view for %s", e_cal_client_get_local_attachment_store(E_CAL_CLIENT(client)));
            auto self = static_cast<Impl*>(gself);
            self->m_views[e_client_get_source(E_CLIENT(client))] = view;//G_CAL_CLIENT(client)] = view;//.insert(view);

            g_signal_connect(view, "objects-added", G_CALLBACK(on_view_objects_added), self);
            g_signal_connect(view, "objects-modified", G_CALLBACK(on_view_objects_modified), self);
            g_signal_connect(view, "objects-removed", G_CALLBACK(on_view_objects_removed), self);
            g_debug("view connected; calling rebuild_soon()");
            self->rebuild_soon();
        }
        else if(error != nullptr)
        {
            if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                g_warning("indicator-datetime cannot get View to EDS client: %s", error->message);

            g_error_free(error);
        }
    }

    static void on_view_objects_added(ECalClientView* /*view*/, gpointer /*objects*/, gpointer gself)
    {
        g_debug("%s", G_STRFUNC);
        static_cast<Impl*>(gself)->rebuild_soon();
    }
    static void on_view_objects_modified(ECalClientView* /*view*/, gpointer /*objects*/, gpointer gself)
    {
        g_debug("%s", G_STRFUNC);
        static_cast<Impl*>(gself)->rebuild_soon();
    }
    static void on_view_objects_removed(ECalClientView* /*view*/, gpointer /*objects*/, gpointer gself)
    {
        g_debug("%s", G_STRFUNC);
        static_cast<Impl*>(gself)->rebuild_soon();
    }

    static void on_source_disabled(ESourceRegistry* /*registry*/, ESource* source, gpointer gself)
    {
        static_cast<Impl*>(gself)->disable_source(source);
    }
    void disable_source(ESource* source)
    {
        // if an ECalClientView is associated with this source, remove it
        auto vit = m_views.find(source);
        if (vit != m_views.end())
        {
            auto& view = vit->second;
            e_cal_client_view_stop(view, nullptr);
            const auto n_disconnected = g_signal_handlers_disconnect_by_data(view, this);
            g_warn_if_fail(n_disconnected == 3);
            g_object_unref(view);
            m_views.erase(vit);
            rebuild_soon();
        }

        // if an ECalClient is associated with this source, remove it
        auto cit = m_clients.find(source);
        if (cit != m_clients.end())
        {
            auto& client = cit->second;
            g_object_unref(client);
            m_clients.erase(cit);
            rebuild_soon();
        }
    }

    static void on_source_removed(ESourceRegistry* /*registry*/, ESource* source, gpointer gself)
    {
        static_cast<Impl*>(gself)->remove_source(source);
    }
    void remove_source(ESource* source)
    {
        disable_source(source);

        auto sit = m_sources.find(source);
        if (sit != m_sources.end())
        {
            g_object_unref(*sit);
            m_sources.erase(sit);
            rebuild_soon();
        }
    }

    static void on_source_changed(ESourceRegistry* /*registry*/, ESource* /*source*/, gpointer gself)
    {
        g_debug("source changed; calling rebuild_soon()");
        static_cast<Impl*>(gself)->rebuild_soon();
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

    void rebuild_soon()
    {
        const static guint ARBITRARY_INTERVAL_SECS = 2;

        if (m_rebuild_tag == 0)
            m_rebuild_tag = g_timeout_add_seconds(ARBITRARY_INTERVAL_SECS, rebuild_now_static, this);
    }

    static gboolean rebuild_now_static(gpointer gself)
    {
        auto self = static_cast<Impl*>(gself);
        self->m_rebuild_tag = 0;
        self->rebuild_now();
        return G_SOURCE_REMOVE;
    }

    void rebuild_now()
    {
        const auto calendar_date = m_owner.time.get().get();
        GDateTime* begin;
        GDateTime* end;
        int y, m, d;

        // get all the appointments in the calendar month
        g_date_time_get_ymd(calendar_date, &y, &m, &d);
        begin = g_date_time_new_local(y, m, 1, 0, 0, 0.1);
        end = g_date_time_new_local(y, m, g_date_get_days_in_month(GDateMonth(m),GDateYear(y)), 23, 59, 59.9);
        if (begin && end)
        {
            get_appointments(begin, end, [this](const std::vector<Appointment>& appointments) {
                g_debug("got %d appointments in this calendar month", (int)appointments.size());
                m_owner.this_month.set(appointments);
            });
        }
        g_clear_pointer(&begin, g_date_time_unref);
        g_clear_pointer(&end, g_date_time_unref);

        // get the upcoming appointments
        begin = g_date_time_ref(calendar_date);
        end = g_date_time_add_months(begin, 1);
        if (begin && end)
        {
            get_appointments(begin, end, [this](const std::vector<Appointment>& appointments) {
                g_debug("got %d upcoming appointments", (int)appointments.size());
                m_owner.upcoming.set(appointments);
            });
        }
        g_clear_pointer(&begin, g_date_time_unref);
        g_clear_pointer(&end, g_date_time_unref);
    }

    void get_appointments(GDateTime* begin_dt, GDateTime* end_dt, appointment_func func)
    {
        const auto begin = g_date_time_to_unix(begin_dt);
        const auto end = g_date_time_to_unix(end_dt);

        auto begin_str = g_date_time_format(begin_dt, "%F %T");
        auto end_str = g_date_time_format(end_dt, "%F %T");
        g_debug("getting all appointments from [%s ... %s]", begin_str, end_str);
        g_free(begin_str);
        g_free(end_str);

        /**
        ***  init the default timezone
        **/

        icaltimezone * default_timezone = nullptr;

        const auto tz = g_date_time_get_timezone_abbreviation(m_owner.time.get().get());
        g_debug("%s tz is %s", G_STRLOC, tz);
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
            g_debug("time to delete task %p", (void*)task);
            task->func(task->appointments);
            delete task;
        });

        for (auto& kv : m_clients)
        {
            auto& client = kv.second;
            if (default_timezone != nullptr)
                e_cal_client_set_default_timezone(client, default_timezone);

            // start a new subtask to enumerate all the components in this client.
            auto& source = kv.first;
            auto extension = e_source_get_extension(source, E_SOURCE_EXTENSION_CALENDAR);
            const auto color = e_source_selectable_get_color(E_SOURCE_SELECTABLE(extension));
            g_debug("calling e_cal_client_generate_instances for %p", (void*)client);
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

              appointment.begin = DateTime(begin);
              appointment.end = DateTime(end);
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

        g_debug("adding appointment '%s' '%s'", subtask->appointment.summary.c_str(), subtask->appointment.url.c_str());
        subtask->task->appointments.push_back(subtask->appointment);
        delete subtask;
    }

    PlannerEds& m_owner;
    std::set<ESource*> m_sources;
    std::map<ESource*,ECalClient*> m_clients;
    std::map<ESource*,ECalClientView*> m_views;
    GCancellable* m_cancellable = nullptr;
    ESourceRegistry* m_source_registry = nullptr;
    guint m_rebuild_tag = 0;
};

PlannerEds::PlannerEds(): p(new Impl(*this)) {}

PlannerEds::~PlannerEds() =default;

} // namespace datetime
} // namespace indicator
} // namespace unity
