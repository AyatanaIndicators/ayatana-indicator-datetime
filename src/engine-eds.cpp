/*
 * Copyright 2014 Canonical Ltd.
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

#include <datetime/engine-eds.h>

#include <libical/ical.h>
#include <libical/icaltime.h>
#include <libecal/libecal.h>
#include <libedataserver/libedataserver.h>

#include <algorithm> // std::sort()
#include <ctime> // time()
#include <map>
#include <set>

namespace unity {
namespace indicator {
namespace datetime {

static constexpr char const * TAG_ALARM {"x-canonical-alarm"};
static constexpr char const * TAG_DISABLED {"x-canonical-disabled"};

/****
*****
****/

class EdsEngine::Impl
{
public:

    Impl():
        m_cancellable(g_cancellable_new())
    {
        e_source_registry_new(m_cancellable, on_source_registry_ready, this);
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

    core::Signal<>& changed()
    {
        return m_changed;
    }

    void get_appointments(const DateTime& begin,
                          const DateTime& end,
                          const Timezone& timezone,
                          std::function<void(const std::vector<Appointment>&)> func)
    {
        const auto begin_timet = begin.to_unix();
        const auto end_timet = end.to_unix();

        const auto b_str = begin.format("%F %T");
        const auto e_str = end.format("%F %T");
        g_debug("getting all appointments from [%s ... %s]", b_str.c_str(), e_str.c_str());

        /**
        ***  init the default timezone
        **/

        icaltimezone * default_timezone = nullptr;
        const auto tz = timezone.timezone.get().c_str();
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

        auto task_deleter = [](Task* task){
            // give the caller the (sorted) finished product
            auto& a = task->appointments;
            std::sort(a.begin(), a.end(), [](const Appointment& a, const Appointment& b){return a.begin < b.begin;});
            task->func(a);
            // we're done; delete the task
            g_debug("time to delete task %p", (void*)task);
            delete task;
        };

        std::shared_ptr<Task> main_task(new Task(this, func), task_deleter);

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
            auto subtask = new AppointmentSubtask(main_task,
                                                  client,
                                                  color,
                                                  default_timezone,
                                                  begin_timet,
                                                  end_timet);
            e_cal_client_generate_instances(client,
                                            begin_timet,
                                            end_timet,
                                            m_cancellable,
                                            my_get_appointments_foreach,
                                            subtask,
                                            [](gpointer g){delete static_cast<AppointmentSubtask*>(g);});
        }
    }

    void disable_ubuntu_alarm(const Appointment& appointment)
    {
        if (appointment.is_ubuntu_alarm())
        {
            for (auto& kv : m_clients) // find the matching icalcomponent
            {
                e_cal_client_get_object(kv.second,
                                        appointment.uid.c_str(),
                                        nullptr,
                                        m_cancellable,
                                        on_object_ready_for_disable,
                                        this);
            }
        }
    }

private:

    void set_dirty_now()
    {
        m_changed();
    }

    static gboolean set_dirty_now_static (gpointer gself)
    {
        auto self = static_cast<Impl*>(gself);
        self->m_rebuild_tag = 0;
        self->m_rebuild_deadline = 0;
        self->set_dirty_now();
        return G_SOURCE_REMOVE;
    }

    void set_dirty_soon()
    {
        static constexpr int MIN_BATCH_SEC = 1;
        static constexpr int MAX_BATCH_SEC = 60;
        static_assert(MIN_BATCH_SEC <= MAX_BATCH_SEC, "bad boundaries");

        const auto now = time(nullptr);

        if (m_rebuild_deadline == 0) // first pass
        {
            m_rebuild_deadline = now + MAX_BATCH_SEC;
            m_rebuild_tag = g_timeout_add_seconds(MIN_BATCH_SEC, set_dirty_now_static, this);
        }
        else if (now < m_rebuild_deadline)
        {
            g_source_remove (m_rebuild_tag);
            m_rebuild_tag = g_timeout_add_seconds(MIN_BATCH_SEC, set_dirty_now_static, this);
        }
    }

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
        ECalClientSourceType source_type;
        bool client_wanted = false;

        if (e_source_has_extension(source, E_SOURCE_EXTENSION_CALENDAR))
        {
            source_type = E_CAL_CLIENT_SOURCE_TYPE_EVENTS;
            client_wanted = true;
        }
        else if (e_source_has_extension(source, E_SOURCE_EXTENSION_TASK_LIST))
        {
            source_type = E_CAL_CLIENT_SOURCE_TYPE_TASKS;
            client_wanted = true;
        }

        const auto source_uid = e_source_get_uid(source);
        if (client_wanted)
        {
            g_debug("%s connecting a client to source %s", G_STRFUNC, source_uid);
            e_cal_client_connect(source,
                                 source_type,
                                 self->m_cancellable,
                                 on_client_connected,
                                 gself);
        }
        else
        {
            g_debug("%s not using source %s -- no tasks/calendar", G_STRFUNC, source_uid);
        }
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

            g_debug("client connected; calling set_dirty_soon()");
            self->set_dirty_soon();
        }
    }

    static void on_client_view_ready (GObject* client, GAsyncResult* res, gpointer gself)
    {
        GError* error = nullptr;
        ECalClientView* view = nullptr;

        if (e_cal_client_get_view_finish (E_CAL_CLIENT(client), res, &view, &error))
        {
            // add the view to our collection
            e_cal_client_view_set_flags(view, E_CAL_CLIENT_VIEW_FLAGS_NONE, nullptr);
            e_cal_client_view_start(view, &error);
            g_debug("got a view for %s", e_cal_client_get_local_attachment_store(E_CAL_CLIENT(client)));
            auto self = static_cast<Impl*>(gself);
            self->m_views[e_client_get_source(E_CLIENT(client))] = view;

            g_signal_connect(view, "objects-added", G_CALLBACK(on_view_objects_added), self);
            g_signal_connect(view, "objects-modified", G_CALLBACK(on_view_objects_modified), self);
            g_signal_connect(view, "objects-removed", G_CALLBACK(on_view_objects_removed), self);
            g_debug("view connected; calling set_dirty_soon()");
            self->set_dirty_soon();
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
        static_cast<Impl*>(gself)->set_dirty_soon();
    }
    static void on_view_objects_modified(ECalClientView* /*view*/, gpointer /*objects*/, gpointer gself)
    {
        g_debug("%s", G_STRFUNC);
        static_cast<Impl*>(gself)->set_dirty_soon();
    }
    static void on_view_objects_removed(ECalClientView* /*view*/, gpointer /*objects*/, gpointer gself)
    {
        g_debug("%s", G_STRFUNC);
        static_cast<Impl*>(gself)->set_dirty_soon();
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
            set_dirty_soon();
        }

        // if an ECalClient is associated with this source, remove it
        auto cit = m_clients.find(source);
        if (cit != m_clients.end())
        {
            auto& client = cit->second;
            g_object_unref(client);
            m_clients.erase(cit);
            set_dirty_soon();
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
            set_dirty_soon();
        }
    }

    static void on_source_changed(ESourceRegistry* /*registry*/, ESource* /*source*/, gpointer gself)
    {
        g_debug("source changed; calling set_dirty_soon()");
        static_cast<Impl*>(gself)->set_dirty_soon();
    }

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
        icaltimezone* default_timezone;
        time_t begin;
        time_t end;

        AppointmentSubtask(const std::shared_ptr<Task>& task_in,
                           ECalClient* client_in,
                           const char* color_in,
                           icaltimezone* default_tz,
                           time_t begin_,
                           time_t end_):
            task(task_in),
            client(client_in),
            default_timezone(default_tz),
            begin(begin_),
            end(end_)
        {
            if (color_in)
                color = color_in;
        }
    };

    static std::string get_alarm_text(ECalComponentAlarm * alarm)
    {
        std::string ret;

        ECalComponentAlarmAction action;
        e_cal_component_alarm_get_action(alarm, &action);
        if (action == E_CAL_COMPONENT_ALARM_DISPLAY)
        {
            ECalComponentText text {};
            e_cal_component_alarm_get_description(alarm, &text);
            if (text.value)
                ret = text.value;
        }

        return ret;
    }

    static std::string get_alarm_sound_url(ECalComponentAlarm * alarm)
    {
        std::string ret;

        ECalComponentAlarmAction action;
        e_cal_component_alarm_get_action(alarm, &action);
        if (action == E_CAL_COMPONENT_ALARM_AUDIO)
        {
            icalattach* attach = nullptr;
            e_cal_component_alarm_get_attach(alarm, &attach);
            if (attach != nullptr)
            {
                if (icalattach_get_is_url (attach))
                {
                    const char* url = icalattach_get_url(attach);
                    if (url != nullptr)
                        ret = url;
                }

                icalattach_unref(attach);
            }
        }

        return ret;
    }

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

            const auto begin_dt = DateTime(begin);
            const auto end_dt = DateTime(end);
            g_debug ("got appointment from %s to %s, uid %s status %d",
                     begin_dt.format("%F %T").c_str(),
                     end_dt.format("%F %T").c_str(),
                     uid,
                     (int)status);

            // look for the in-house tags
            bool disabled = false;
            Appointment::Type type = Appointment::EVENT;
            GSList * categ_list = nullptr;
            e_cal_component_get_categories_list (component, &categ_list);
            for (GSList * l=categ_list; l!=nullptr; l=l->next) {
                auto tag = static_cast<const char*>(l->data);
                if (!g_strcmp0(tag, TAG_ALARM))
                    type = Appointment::UBUNTU_ALARM;
                if (!g_strcmp0(tag, TAG_DISABLED))
                    disabled = true;
            }
            e_cal_component_free_categories_list(categ_list);

            if ((uid != nullptr) &&
                (!disabled) &&
                (status != ICAL_STATUS_COMPLETED) &&
                (status != ICAL_STATUS_CANCELLED))
            {
                ECalComponentAlarmAction omit[] = { (ECalComponentAlarmAction)-1 }; // list of action types to omit, terminated with -1
                Appointment appointment;

                ECalComponentText text {};
                e_cal_component_get_summary(component, &text);
                if (text.value)
                    appointment.summary = text.value;

                appointment.begin = begin_dt;
                appointment.end = end_dt;
                appointment.color = subtask->color;
                appointment.uid = uid;
                appointment.type = type;

                icalcomponent * icc = e_cal_component_get_icalcomponent(component);
                g_debug("%s", icalcomponent_as_ical_string(icc)); // libical owns this string; no leak

                auto e_alarms = e_cal_util_generate_alarms_for_comp(component,
                                                                    subtask->begin,
                                                                    subtask->end,
                                                                    omit,
                                                                    e_cal_client_resolve_tzid_cb,
                                                                    subtask->client,
                                                                    subtask->default_timezone);

                std::map<DateTime,Alarm> alarms;

                if (e_alarms != nullptr)
                {
                    for (auto l=e_alarms->alarms; l!=nullptr; l=l->next)
                    {
                        auto ai = static_cast<ECalComponentAlarmInstance*>(l->data);
                        auto a = e_cal_component_get_alarm(component, ai->auid);

                        if (a != nullptr)
                        {
                            const DateTime alarm_begin{ai->occur_start};
                            auto& alarm = alarms[alarm_begin];

                            if (alarm.text.empty())
                                alarm.text = get_alarm_text(a);
                            if (alarm.audio_url.empty())
                                alarm.audio_url = get_alarm_sound_url(a);
                            if (!alarm.time.is_set())
                                alarm.time = alarm_begin;
                            if (alarm.duration == std::chrono::seconds::zero())
                                alarm.duration = std::chrono::seconds(ai->occur_end - ai->occur_start);

                            e_cal_component_alarm_free(a);
                        }
                    }

                    e_cal_component_alarms_free(e_alarms);
                }
                // hm, no alarms? if this came from ubuntu-clock-app,
                // manually add a single alarm for the todo event's time
                else if (appointment.is_ubuntu_alarm())
                {
                    Alarm tmp;
                    tmp.time = appointment.begin;

                    auto auids = e_cal_component_get_alarm_uids(component);
                    for(auto l=auids; l!=nullptr; l=l->next)
                    {
                        const auto auid = static_cast<const char*>(l->data);
                        auto a = e_cal_component_get_alarm(component, auid);
                        if (a != nullptr)
                        {
                            if (tmp.text.empty())
                                tmp.text = get_alarm_text(a);
                            if (tmp.audio_url.empty())
                                tmp.audio_url = get_alarm_sound_url(a);
                            e_cal_component_alarm_free(a);
                        }
                    }
                    cal_obj_uid_list_free(auids);

                    alarms[tmp.time] = tmp;
                }

                appointment.alarms.reserve(alarms.size());
                for (const auto& it : alarms)
                    appointment.alarms.push_back(it.second);

                subtask->task->appointments.push_back(appointment);
            }
        }
 
        return G_SOURCE_CONTINUE;
    }

    /***
    ****
    ***/

    static void on_object_ready_for_disable(GObject      * client,
                                            GAsyncResult * result,
                                            gpointer       gself)
    {
        icalcomponent * icc = nullptr;
        if (e_cal_client_get_object_finish (E_CAL_CLIENT(client), result, &icc, nullptr))
        {
            auto rrule_property = icalcomponent_get_first_property (icc, ICAL_RRULE_PROPERTY); // transfer none
            auto rdate_property = icalcomponent_get_first_property (icc, ICAL_RDATE_PROPERTY); // transfer none
            const bool is_nonrepeating = (rrule_property == nullptr) && (rdate_property == nullptr);

            if (is_nonrepeating)
            {
                g_debug("'%s' appears to be a one-time alarm... adding 'disabled' tag.",
                        icalcomponent_as_ical_string(icc));

                auto ecc = e_cal_component_new_from_icalcomponent (icc); // takes ownership of icc
                icc = nullptr;

                if (ecc != nullptr)
                {
                    // add TAG_DISABLED to the list of categories
                    GSList * old_categories = nullptr;
                    e_cal_component_get_categories_list(ecc, &old_categories);
                    auto new_categories = g_slist_copy(old_categories);
                    new_categories = g_slist_append(new_categories, const_cast<char*>(TAG_DISABLED));
                    e_cal_component_set_categories_list(ecc, new_categories);
                    g_slist_free(new_categories);
                    e_cal_component_free_categories_list(old_categories);
                    e_cal_client_modify_object(E_CAL_CLIENT(client),
                                               e_cal_component_get_icalcomponent(ecc),
                                               E_CAL_OBJ_MOD_THIS,
                                               static_cast<Impl*>(gself)->m_cancellable,
                                               on_disable_done,
                                               nullptr);

                    g_clear_object(&ecc);
                }
            }

            g_clear_pointer(&icc, icalcomponent_free);
        }
    }

    static void on_disable_done (GObject* gclient, GAsyncResult *res, gpointer)
    {
        GError * error = nullptr;
        if (!e_cal_client_modify_object_finish (E_CAL_CLIENT(gclient), res, &error))
        {
            if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                g_warning("indicator-datetime cannot mark one-time alarm as disabled: %s", error->message);

            g_error_free(error);
        }
    }

    /***
    ****
    ***/
 
    core::Signal<> m_changed;
    std::set<ESource*> m_sources;
    std::map<ESource*,ECalClient*> m_clients;
    std::map<ESource*,ECalClientView*> m_views;
    GCancellable* m_cancellable {};
    ESourceRegistry* m_source_registry {};
    guint m_rebuild_tag {};
    time_t m_rebuild_deadline {};
};

/***
****
***/

EdsEngine::EdsEngine():
    p(new Impl())
{
}

EdsEngine::~EdsEngine() =default;

core::Signal<>& EdsEngine::changed()
{
    return p->changed();
}

void EdsEngine::get_appointments(const DateTime& begin,
                                 const DateTime& end,
                                 const Timezone& tz,
                                 std::function<void(const std::vector<Appointment>&)> func)
{
    p->get_appointments(begin, end, tz, func);
}

void EdsEngine::disable_ubuntu_alarm(const Appointment& appointment)
{
    p->disable_ubuntu_alarm(appointment);
}

/***
****
***/

} // namespace datetime
} // namespace indicator
} // namespace unity
