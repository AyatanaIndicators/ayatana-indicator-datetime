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

#include <notifications/notifications.h>

#include <libnotify/notify.h>

#include <map>
#include <set>
#include <string>
#include <vector>

namespace unity {
namespace indicator {
namespace notifications {

static G_DEFINE_QUARK(NotificationKey, notification_key)

static G_DEFINE_QUARK(NotificationAction, notification_action)

/***
****
***/

class Builder::Impl
{
public:
    std::string m_title;
    std::string m_body;
    std::string m_icon_name;
    std::chrono::seconds m_duration;
    std::set<std::string> m_string_hints;
    std::vector<std::pair<std::string,std::string>> m_actions;
    std::function<void(const std::string&)> m_closed_callback;
};

Builder::Builder():
  impl(new Impl())
{
}

Builder::~Builder()
{
}
 
void
Builder::set_title (const std::string& title)
{
  impl->m_title = title;
}

void
Builder::set_body (const std::string& body)
{
  impl->m_body = body;
}

void
Builder::set_icon_name (const std::string& icon_name)
{
  impl->m_icon_name = icon_name;
}

void
Builder::set_timeout (const std::chrono::seconds& duration)
{
  impl->m_duration = duration;
}

void
Builder::add_hint (const std::string& name)
{
  impl->m_string_hints.insert (name);
}

void
Builder::add_action (const std::string& action, const std::string& label)
{
  impl->m_actions.push_back(std::pair<std::string,std::string>(action,label));
}

void
Builder::set_closed_callback (std::function<void (const std::string&)> cb)
{
  impl->m_closed_callback.swap (cb);
}

/***
****
***/

class Engine::Impl
{
    struct notification_data
    {
        std::shared_ptr<NotifyNotification> nn;
        std::function<void(const std::string&)> closed_callback;
    };

public:

    Impl(const std::string& app_name):
        m_app_name(app_name)
    {
        if (!notify_init(app_name.c_str()))
            g_critical("Unable to initialize libnotify!");

        // put the server capabilities into m_caps
        auto caps_gl = notify_get_server_caps();
        std::string caps_str;
        for(auto l=caps_gl; l!=nullptr; l=l->next)
        {
            m_caps.insert((const char*)l->data);

            caps_str += (const char*) l->data;;
            if (l->next != nullptr)
                caps_str += ", ";
        }

        g_debug("%s notify_get_server() returned [%s]", G_STRFUNC, caps_str.c_str());
        g_list_free_full(caps_gl, g_free);
    }

    ~Impl()
    {
        close_all ();

        notify_uninit ();
    }

    const std::string& app_name() const
    {
        return m_app_name;
    }

    bool supports_actions() const
    {
        return m_caps.count("actions") != 0;
    }

    void close_all ()
    {
        // close() removes the item from m_notifications,
        // so increment the iterator before it gets invalidated
        for (auto it=m_notifications.begin(), e=m_notifications.end(); it!=e; )
        {
          const int key = it->first;
          ++it;
          close (key);
        }
    }

    void close (int key)
    {
        // if we've got this one...
        auto it = m_notifications.find(key);
        if (it != m_notifications.end())
        {
            GError * error = nullptr;
            if (!notify_notification_close (it->second.nn.get(), &error))
            {
                g_warning ("Unable to close notification %d: %s", key, error->message);
                g_error_free (error);
            }
            on_closed (key);
        }
    }

    int show (const Builder& builder)
    {
        int ret = -1;
        const auto& info = *builder.impl;

        auto * nn = notify_notification_new (info.m_title.c_str(),
                                             info.m_body.c_str(),
                                             info.m_icon_name.c_str());

        if (info.m_duration.count() != 0)
        {
            const auto& d= info.m_duration;
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(d);

            notify_notification_set_hint (nn,
                                          HINT_TIMEOUT,
                                          g_variant_new_int32(ms.count()));
        }

        for (const auto& hint : info.m_string_hints)
        {
            notify_notification_set_hint (nn,
                                          hint.c_str(),
                                          g_variant_new_boolean(true));
        }

        for (const auto& action : info.m_actions)
        {
            notify_notification_add_action (nn,
                                            action.first.c_str(),
                                            action.second.c_str(),
                                            on_notification_clicked,
                                            nullptr,
                                            nullptr);
        }

        // if we can show it, keep it
        GError * error = nullptr;
        if (notify_notification_show(nn, &error))
        {
            static int next_key = 1;
            const int key = next_key++;

            g_signal_connect (nn, "closed",
                              G_CALLBACK(on_notification_closed), this);
            g_object_set_qdata (G_OBJECT(nn),
                                notification_key_quark(),
                                GINT_TO_POINTER(key));

            notification_data ndata;
            ndata.closed_callback = info.m_closed_callback;
            ndata.nn.reset(nn, [this](NotifyNotification * n) {
                g_signal_handlers_disconnect_by_data(n, this);
                g_object_unref (G_OBJECT(n));
            });

            m_notifications[key] = ndata;
            ret = key;
        }
        else
        {
            g_critical ("Unable to show notification for '%s': %s", info.m_title.c_str(), error->message);
            g_error_free (error);
            g_object_unref (nn);
        }

        return ret;
    }

private:

    static void on_notification_clicked (NotifyNotification * nn,
                                         char               * action,
                                         gpointer)
    {
        g_object_set_qdata_full (G_OBJECT(nn),
                                 notification_action_quark(),
                                 g_strdup(action),
                                 g_free);
    }

    static void on_notification_closed (NotifyNotification * nn, gpointer gself)
    {
        const GQuark q = notification_key_quark();
        const int key = GPOINTER_TO_INT(g_object_get_qdata(G_OBJECT(nn), q));
        static_cast<Impl*>(gself)->on_closed (key);
    }

    void on_closed (int key)
    {
        auto it = m_notifications.find(key);
        g_return_if_fail (it != m_notifications.end());

        const auto& ndata = it->second;
        auto nn = ndata.nn.get();
        if (ndata.closed_callback)
        {
            std::string action;

            const auto q = notification_action_quark();
            const auto p = g_object_get_qdata (G_OBJECT(nn), q);
            if (p != nullptr)
                action = static_cast<const char*>(p);

            ndata.closed_callback (action);
        }

        g_signal_handlers_disconnect_by_data(nn, this);
        m_notifications.erase(it);
    }

    /***
    ****
    ***/

    const std::string m_app_name;
    std::map<int,notification_data> m_notifications;
    std::set<std::string> m_caps;

    static constexpr char const * HINT_TIMEOUT {"x-canonical-snap-decisions-timeout"};
};

/***
****
***/

Engine::Engine(const std::string& app_name):
    impl(new Impl(app_name))
{
}

Engine::~Engine()
{
}

bool
Engine::supports_actions() const
{
    return impl->supports_actions();
}

int
Engine::show(const Builder& builder)
{
    return impl->show(builder);
}

void
Engine::close_all()
{
    impl->close_all();
}

void
Engine::close(int key)
{
    impl->close(key);
}

const std::string&
Engine::app_name() const
{
    return impl->app_name();
}

/***
****
***/

} // namespace notifications
} // namespace indicator
} // namespace unity

