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
#include <array>
#include <ctime> // time()
#include <map>
#include <set>

namespace unity {
namespace indicator {
namespace datetime {

static constexpr char const * TAG_ALARM    {"x-canonical-alarm"};
static constexpr char const * TAG_DISABLED {"x-canonical-disabled"};

static constexpr char const * X_PROP_ACTIVATION_URL {"X-CANONICAL-ACTIVATION-URL"};

/****
*****
****/

class EdsEngine::Impl
{
public:

    Impl()
    {
        auto cancellable_deleter = [](GCancellable * c) {
            g_cancellable_cancel(c);
            g_clear_object(&c);
        };

        m_cancellable = std::shared_ptr<GCancellable>(g_cancellable_new(), cancellable_deleter);

        e_source_registry_new(m_cancellable.get(), on_source_registry_ready, this);
    }

    ~Impl()
    {
        m_cancellable.reset();

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

        auto gtz = default_timezone != nullptr
                 ? g_time_zone_new(icaltimezone_get_location(default_timezone))
                 : g_time_zone_new_local(); 
        auto main_task = std::make_shared<Task>(this, func, default_timezone, gtz, begin, end);

        for (auto& kv : m_clients)
        {
            auto& client = kv.second;
            if (default_timezone != nullptr)
                e_cal_client_set_default_timezone(client, default_timezone);
            g_debug("calling e_cal_client_generate_instances for %p", (void*)client);

            auto& source = kv.first;
            auto extension = e_source_get_extension(source, E_SOURCE_EXTENSION_CALENDAR);
            const auto color = e_source_selectable_get_color(E_SOURCE_SELECTABLE(extension));

            auto begin_str = isodate_from_time_t(begin.to_unix());
            auto end_str = isodate_from_time_t(end.to_unix());
            auto sexp_fmt = g_strdup_printf("(%%s? (make-time \"%s\") (make-time \"%s\"))", begin_str, end_str);
            g_clear_pointer(&begin_str, g_free);
            g_clear_pointer(&end_str, g_free);

            // ask EDS about alarms that occur in this window...
            auto sexp = g_strdup_printf(sexp_fmt, "has-alarms-in-range");
            g_debug("%s alarm sexp is %s", G_STRLOC, sexp);
            e_cal_client_get_object_list_as_comps(
                client,
                sexp,
                m_cancellable.get(),
                on_alarm_component_list_ready,
                new ClientSubtask(main_task, client, m_cancellable, color));
            g_clear_pointer(&sexp, g_free);

            // ask EDS about events that occur in this window...
            sexp = g_strdup_printf(sexp_fmt, "occur-in-time-range");
            g_debug("%s event sexp is %s", G_STRLOC, sexp);
            e_cal_client_get_object_list_as_comps(
                client,
                sexp,
                m_cancellable.get(),
                on_event_component_list_ready,
                new ClientSubtask(main_task, client, m_cancellable, color));
            g_clear_pointer(&sexp, g_free);

            g_clear_pointer(&sexp_fmt, g_free);
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
                                        m_cancellable.get(),
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
                                 self->m_cancellable.get(),
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
            auto source = e_client_get_source(client);
            auto ecc = E_CAL_CLIENT(client);
            self->m_clients[source] = ecc;

            self->ensure_client_alarms_have_triggers(ecc);

            // now create a view for it so that we can listen for changes
            e_cal_client_get_view (ecc,
                                   "#t", // match all
                                   self->m_cancellable.get(),
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

    /***
    ****
    ***/

    // old ubuntu-clock-app alarms created VTODO VALARMS without the
    // required 'TRIGGER' property... http://pad.lv/1465806

    void ensure_client_alarms_have_triggers(ECalClient* client)
    {
        // ask the EDS server for all the ubuntu-clock-app alarms...

        auto sexp = g_strdup_printf("has-categories? '%s'", TAG_ALARM);

        e_cal_client_get_object_list_as_comps(
            client,
            sexp,
            m_cancellable.get(),
            ensure_client_alarms_have_triggers_async_cb,
            this);

        g_clear_pointer(&sexp, g_free);
    }

    static void ensure_client_alarms_have_triggers_async_cb(
        GObject      * oclient,
        GAsyncResult * res,
        gpointer       gself)
    {
        ECalClient * client = E_CAL_CLIENT(oclient);
        GError * error = nullptr;
        GSList * components = nullptr;

        if (e_cal_client_get_object_list_as_comps_finish(client,
                                                         res,
                                                         &components,
                                                         &error))
        {
            auto self = static_cast<Impl*>(gself);
            self->ensure_canonical_alarms_have_triggers(client, components);
            e_cal_client_free_ecalcomp_slist(components);
        }
        else if (error != nullptr)
        {
            if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                g_warning("can't get clock-app alarm list: %s", error->message);

            g_error_free(error);
        }
    }

    void ensure_canonical_alarms_have_triggers(ECalClient * client,
                                               GSList     * components)
    {
        GSList * modify_slist = nullptr;

        // for each component..
        for (auto l=components; l!=nullptr; l=l->next)
        {
            bool changed = false;

            // for each alarm...
            auto component = E_CAL_COMPONENT(l->data);
            auto auids = e_cal_component_get_alarm_uids(component);
            for(auto l=auids; l!=nullptr; l=l->next)
            {
                auto auid = static_cast<const char*>(l->data);
                auto alarm = e_cal_component_get_alarm(component, auid);
                if (alarm == nullptr)
                    continue;

                // if the alarm has no trigger, add one.
                ECalComponentAlarmTrigger trigger;
                e_cal_component_alarm_get_trigger(alarm, &trigger);
                if (trigger.type == E_CAL_COMPONENT_ALARM_TRIGGER_NONE)
                {
                    trigger.type = E_CAL_COMPONENT_ALARM_TRIGGER_RELATIVE_START;
                    trigger.u.rel_duration = icaldurationtype_from_int(0);
                    e_cal_component_alarm_set_trigger (alarm, trigger);
                    changed = true;
                }

                g_clear_pointer(&alarm, e_cal_component_alarm_free);
            }
            g_clear_pointer(&auids, cal_obj_uid_list_free);

            if (changed)
            {
                auto icc = e_cal_component_get_icalcomponent(component); // icc owned by ecc
                modify_slist = g_slist_prepend(modify_slist, icc);
            }
        }

        if (modify_slist != nullptr)
        {
            e_cal_client_modify_objects(client,
                                        modify_slist,
                                        E_CAL_OBJ_MOD_ALL,
                                        m_cancellable.get(),
                                        ensure_canonical_alarms_have_triggers_async_cb,
                                        this);

            g_clear_pointer(&modify_slist, g_slist_free);
        }
    }

    // log a warning if e_cal_client_modify_objects() failed
    static void ensure_canonical_alarms_have_triggers_async_cb(
        GObject      * oclient,
        GAsyncResult * res,
        gpointer       /*gself*/)
    {
        GError * error = nullptr;

        e_cal_client_modify_objects_finish (E_CAL_CLIENT(oclient), res, &error);

        if (error != nullptr)
        {
            if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                g_warning("couldn't add alarm triggers: %s", error->message);

            g_error_free(error);
        }
    }

    /***
    ****
    ***/


    typedef std::function<void(const std::vector<Appointment>&)> appointment_func;

    struct Task
    {
        Impl* p;
        appointment_func func;
        icaltimezone* default_timezone; // pointer owned by libical
        GTimeZone* gtz;
        std::vector<Appointment> appointments;
        const DateTime begin;
        const DateTime end;

        Task(Impl* p_in,
             appointment_func func_in,
             icaltimezone* tz_in,
             GTimeZone* gtz_in,
             const DateTime& begin_in,
             const DateTime& end_in):
                 p{p_in},
                 func{func_in},
                 default_timezone{tz_in},
                 gtz{gtz_in},
                 begin{begin_in},
                 end{end_in} {}

        ~Task() {
            g_clear_pointer(&gtz, g_time_zone_unref);
            // give the caller the sorted finished product
            auto& a = appointments;
            std::sort(a.begin(), a.end(), [](const Appointment& a, const Appointment& b){return a.begin < b.begin;});
            func(a);
        };
    };

    struct ClientSubtask
    {
        std::shared_ptr<Task> task;
        ECalClient* client;
        std::shared_ptr<GCancellable> cancellable;
        std::string color;

        ClientSubtask(const std::shared_ptr<Task>& task_in,
                      ECalClient* client_in,
                      const std::shared_ptr<GCancellable>& cancellable_in,
                      const char* color_in):
            task(task_in),
            client(client_in),
            cancellable(cancellable_in)
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

    static void
    on_alarm_component_list_ready(GObject      * oclient,
                                  GAsyncResult * res,
                                  gpointer       gsubtask)
    {
        GError * error = NULL;
        GSList * comps_slist = NULL;
        auto subtask = static_cast<ClientSubtask*>(gsubtask);

        if (e_cal_client_get_object_list_as_comps_finish(E_CAL_CLIENT(oclient),
                                                         res,
                                                         &comps_slist,
                                                         &error))
        {
            // _generate_alarms takes a GList, so make a shallow one
            GList * comps_list = nullptr;
            for (auto l=comps_slist; l!=nullptr; l=l->next)
                comps_list = g_list_prepend(comps_list, l->data);

            constexpr std::array<ECalComponentAlarmAction,1> omit = {
                (ECalComponentAlarmAction)-1
            }; // list of action types to omit, terminated with -1
            GSList * comp_alarms = nullptr;
            e_cal_util_generate_alarms_for_list(
                comps_list,
                subtask->task->begin.to_unix(),
                subtask->task->end.to_unix(),
                const_cast<ECalComponentAlarmAction*>(omit.data()),
                &comp_alarms,
                e_cal_client_resolve_tzid_cb,
                oclient,
                subtask->task->default_timezone);

            // walk the alarms & add them
            for (auto l=comp_alarms; l!=nullptr; l=l->next)
                add_alarms_to_subtask(static_cast<ECalComponentAlarms*>(l->data), subtask, subtask->task->gtz);

            // cleanup
            e_cal_free_alarms(comp_alarms);
            g_list_free(comps_list);
            e_cal_client_free_ecalcomp_slist(comps_slist);
        }
        else if (error != nullptr)
        {
            if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                g_warning("can't get ecalcomponent list: %s", error->message);

            g_error_free(error);
        }

        delete subtask;
    }

    static void
    on_event_component_list_ready(GObject      * oclient,
                                  GAsyncResult * res,
                                  gpointer       gsubtask)
    {
        GError * error = NULL;
        GSList * comps_slist = NULL;
        auto subtask = static_cast<ClientSubtask*>(gsubtask);

        if (e_cal_client_get_object_list_as_comps_finish(E_CAL_CLIENT(oclient),
                                                         res,
                                                         &comps_slist,
                                                         &error))
        {
            for (auto l=comps_slist; l!=nullptr; l=l->next)
                add_event_to_subtask(static_cast<ECalComponent*>(l->data), subtask, subtask->task->gtz);

            e_cal_client_free_ecalcomp_slist(comps_slist);
        }
        else if (error != nullptr)
        {
            if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                g_warning("can't get ecalcomponent list: %s", error->message);

            g_error_free(error);
        }

        delete subtask;
    }

    static DateTime
    datetime_from_component_date_time(ECalClient                     * client,
                                      std::shared_ptr<GCancellable>  & cancellable,
                                      const ECalComponentDateTime    & in,
                                      GTimeZone                      * default_timezone)
    {
        DateTime out;
        g_return_val_if_fail(in.value != nullptr, out);

        GTimeZone * gtz {};
        if (in.tzid != nullptr)
        {
            auto itz = icaltimezone_get_builtin_timezone_from_tzid(in.tzid); // usually works

            if (itz == nullptr) // fallback
                itz = icaltimezone_get_builtin_timezone(in.tzid);

            if (itz == nullptr) // ok we have a strange tzid... ask EDS to look it up in VTIMEZONES
                e_cal_client_get_timezone_sync(client, in.tzid, &itz, cancellable.get(), nullptr);

            const char * tzid;
            if (itz != nullptr)
            {
                tzid = icaltimezone_get_location(itz);
            }
            else
            {
                g_warning("Unrecognized TZID: '%s'", in.tzid);
                tzid = nullptr;
            }

            gtz = g_time_zone_new(tzid);
            g_debug("%s eccdt.tzid -> offset is %d", G_STRLOC, in.tzid, (int)g_time_zone_get_offset(gtz,0));
        }
        else
        {
            gtz = g_time_zone_ref(default_timezone);
        }

        out = DateTime(gtz,
                       in.value->year,
                       in.value->month,
                       in.value->day,
                       in.value->hour,
                       in.value->minute,
                       in.value->second);
        g_time_zone_unref(gtz);
        return out;
    }

    static bool
    is_component_interesting(ECalComponent * component)
    {
        // we only want calendar events and vtodos
        const auto vtype = e_cal_component_get_vtype(component);
        if ((vtype != E_CAL_COMPONENT_EVENT) &&
                (vtype != E_CAL_COMPONENT_TODO))
            return false;

        // we're not interested in completed or cancelled components
        auto status = ICAL_STATUS_NONE;
        e_cal_component_get_status(component, &status);
        if ((status == ICAL_STATUS_COMPLETED) ||
                (status == ICAL_STATUS_CANCELLED))
            return false;

        // we don't want disabled alarms
        bool disabled = false;
        GSList * categ_list = nullptr;
        e_cal_component_get_categories_list (component, &categ_list);
        for (GSList * l=categ_list; l!=nullptr; l=l->next) {
            auto tag = static_cast<const char*>(l->data);
            if (!g_strcmp0(tag, TAG_DISABLED))
                disabled = true;
        }
        e_cal_component_free_categories_list(categ_list);
        if (disabled)
            return false;

        return true;
    }

    static Appointment
    get_appointment(ECalClient                    * client,
                    std::shared_ptr<GCancellable> & cancellable,
                    ECalComponent                 * component,
                    GTimeZone                     * gtz)
    {
        Appointment baseline;

        // get appointment.uid
        const gchar* uid = nullptr;
        e_cal_component_get_uid(component, &uid);
        if (uid != nullptr)
            baseline.uid = uid;

        // get appointment.summary
        ECalComponentText text {};
        e_cal_component_get_summary(component, &text);
        if (text.value)
            baseline.summary = text.value;

        // get appointment.begin
        ECalComponentDateTime eccdt_tmp {};
        e_cal_component_get_dtstart(component, &eccdt_tmp);
        baseline.begin = datetime_from_component_date_time(client, cancellable, eccdt_tmp, gtz);
        e_cal_component_free_datetime(&eccdt_tmp);

        // get appointment.end
        e_cal_component_get_dtend(component, &eccdt_tmp);
        baseline.end = eccdt_tmp.value != nullptr
                                  ? datetime_from_component_date_time(client, cancellable, eccdt_tmp, gtz)
                                  : baseline.begin;
        e_cal_component_free_datetime(&eccdt_tmp);

        // get appointment.activation_url from x-props
        auto icc = e_cal_component_get_icalcomponent(component); // icc owned by component
        auto icalprop = icalcomponent_get_first_property(icc, ICAL_X_PROPERTY);
        while (icalprop != nullptr) {
            const char * x_name = icalproperty_get_x_name(icalprop);
            if ((x_name != nullptr) && !g_ascii_strcasecmp(x_name, X_PROP_ACTIVATION_URL)) {
                const char * url = icalproperty_get_value_as_string(icalprop);
                if ((url != nullptr) && baseline.activation_url.empty())
                    baseline.activation_url = url;
            }
            icalprop = icalcomponent_get_next_property(icc, ICAL_X_PROPERTY);
        }

        // get appointment.type
        baseline.type = Appointment::EVENT;
        GSList * categ_list = nullptr;
        e_cal_component_get_categories_list (component, &categ_list);
        for (GSList * l=categ_list; l!=nullptr; l=l->next) {
            auto tag = static_cast<const char*>(l->data);
            if (!g_strcmp0(tag, TAG_ALARM))
                baseline.type = Appointment::UBUNTU_ALARM;
        }
        e_cal_component_free_categories_list(categ_list);

        g_debug("%s got appointment from %s to %s: %s", G_STRLOC,
                baseline.begin.format("%F %T %z").c_str(),
                baseline.end.format("%F %T %z").c_str(),
                icalcomponent_as_ical_string(icc) /* string owned by ical */);

        return baseline;
    }

    static void
    add_event_to_subtask(ECalComponent * component,
                         ClientSubtask * subtask,
                         GTimeZone     * gtz)
    {
        // events with alarms are covered by add_alarms_to_subtask(),
        // so skip them here
        auto auids = e_cal_component_get_alarm_uids(component);
        const bool has_alarms = auids != nullptr;
        cal_obj_uid_list_free(auids);
        if (has_alarms)
            return;

        // add it. simple, eh?
        if (is_component_interesting(component))
        {
            Appointment appointment = get_appointment(subtask->client, subtask->cancellable, component, gtz);
            appointment.color = subtask->color;
            subtask->task->appointments.push_back(appointment);
        }
    }

    static void
    add_alarms_to_subtask(ECalComponentAlarms * comp_alarms,
                          ClientSubtask       * subtask,
                          GTimeZone           * gtz)
    {
        auto& component = comp_alarms->comp;

        if (!is_component_interesting(component))
            return;

        Appointment baseline = get_appointment(subtask->client, subtask->cancellable, component, gtz);
        baseline.color = subtask->color;

        /**
        ***  Now loop through comp_alarms to get information that we need
        ***  to build the instance appointments and their alarms.
        ***
        ***  Outer map key is the instance component's start + end time.
        ***  We build Appointment.begin and .end from that.
        ***
        ***  inner map key is the alarm trigger, we build Alarm.time from that.
        ***
        ***  inner map value is the Alarm.
        ***
        ***  We map the alarms based on their trigger time so that we
        ***  can fold together multiple valarms that trigger for the
        ***  same componeng at the same time. This is commonplace;
        ***  e.g. one valarm will have a display action and another
        ***  will specify a sound to be played.
         */
        std::map<std::pair<DateTime,DateTime>,std::map<DateTime,Alarm>> alarms;
        for (auto l=comp_alarms->alarms; l!=nullptr; l=l->next)
        {
            auto ai = static_cast<ECalComponentAlarmInstance*>(l->data);
            auto a = e_cal_component_get_alarm(component, ai->auid);
            if (a == nullptr)
                continue;

            auto instance_time = std::make_pair(DateTime{gtz, ai->occur_start},
                                                DateTime{gtz, ai->occur_end});
            auto trigger_time = DateTime{gtz, ai->trigger};

            auto& alarm = alarms[instance_time][trigger_time];

            if (alarm.text.empty())
                alarm.text = get_alarm_text(a);
            if (alarm.audio_url.empty())
                alarm.audio_url = get_alarm_sound_url(a);
            if (!alarm.time.is_set())
                alarm.time = trigger_time;

            e_cal_component_alarm_free(a);
        }

        for (auto& i : alarms)
        {
            Appointment appointment = baseline;
            appointment.begin = i.first.first;
            appointment.end = i.first.second;
            appointment.alarms.reserve(i.second.size());
            for (auto& j : i.second)
                appointment.alarms.push_back(j.second);
            subtask->task->appointments.push_back(appointment);
        }
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
                                               static_cast<Impl*>(gself)->m_cancellable.get(),
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
    std::shared_ptr<GCancellable> m_cancellable;
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
