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

#include <messaging-menu/messaging-menu-app.h>
#include <messaging-menu/messaging-menu-message.h>

#ifdef HAS_LOMIRIAPPLAUNCH
#include <liblomiri-app-launch/lomiri-app-launch/appid.h>
#include <liblomiri-app-launch/lomiri-app-launch/registry.h>
#endif

#include <uuid/uuid.h>

#include <gio/gdesktopappinfo.h>

#include <map>
#include <set>
#include <string>
#include <vector>
#include <memory>


namespace ayatana {
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
    gint64 m_start_time  {};
    std::set<std::string> m_string_hints;
    std::vector<std::pair<std::string,std::string>> m_actions;
    std::function<void(const std::string&)> m_closed_callback;
    std::function<void()> m_timeout_callback;
    bool m_show_notification_bubble;
    bool m_post_to_messaging_menu;
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

void
Builder::set_timeout_callback (std::function<void()> cb)
{
  impl->m_timeout_callback.swap (cb);
}

void
Builder::set_start_time (uint64_t time)
{
  impl->m_start_time = time;
}

void
Builder::set_show_notification_bubble (bool show)
{
  impl->m_show_notification_bubble = show;
}

void
Builder::set_post_to_messaging_menu (bool post)
{
  impl->m_post_to_messaging_menu = post;
}

/***
****
***/

class Engine::Impl
{
    struct notification_data
    {
        std::shared_ptr<NotifyNotification> nn;
        Builder::Impl data;
    };

    struct messaging_menu_data
    {
        std::string msg_id;
        std::function<void()> callback;
        Engine::Impl *self;
    };

public:

    Impl(const std::string& app_name):
        m_app_name(app_name)
    {
        if (!notify_init(app_name.c_str()))
            g_critical("Unable to initialize libnotify!");

        // messaging menu
        auto app_id = calendar_app_id();
        if (!app_id.empty()) {
            m_messaging_app.reset(messaging_menu_app_new(app_id.c_str()), g_object_unref);
            messaging_menu_app_register(m_messaging_app.get());
        }
    }

    ~Impl()
    {
        close_all ();
        remove_all ();

        notify_uninit ();
        if (m_messaging_app)
            messaging_menu_app_unregister (m_messaging_app.get());
    }

    const std::string& app_name() const
    {
        return m_app_name;
    }

    bool supports_actions() const
    {
        return server_caps().count("actions") != 0;
    }

    void close_all ()
    {
        // call close() on all our keys

        std::set<int> keys;
        for (const auto& it : m_notifications)
            keys.insert (it.first);

        for (const int key : keys)
            close (key);
    }

    void close (int key)
    {
        auto it = m_notifications.find(key);
        if (it != m_notifications.end())
        {
            // tell the server to close the notification
            GError * error = nullptr;
            if (!notify_notification_close (it->second.nn.get(), &error))
            {
                g_warning ("Unable to close notification %d: %s", key, error->message);
                g_error_free (error);
            }

            // call the user callback and remove it from our bookkeeping
            remove_closed_notification (key);
        }
    }

    int show (const Builder& builder)
    {
        int ret = -1;
        const auto& info = *builder.impl;

        std::shared_ptr<NotifyNotification> nn (
            notify_notification_new(info.m_title.c_str(),
                                    info.m_body.c_str(),
                                    info.m_icon_name.c_str()),
            [this](NotifyNotification * n) {
                g_signal_handlers_disconnect_by_data(n, this);
                g_object_unref (G_OBJECT(n));
            }
        );

        if (info.m_duration.count() != 0)
        {
            const auto& d= info.m_duration;
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(d);

            notify_notification_set_hint (nn.get(),
                                          HINT_TIMEOUT,
                                          g_variant_new_int32(ms.count()));
        }

        for (const auto& hint : info.m_string_hints)
        {
            notify_notification_set_hint (nn.get(),
                                          hint.c_str(),
                                          g_variant_new_string("true"));
        }

        for (const auto& action : info.m_actions)
        {
            notify_notification_add_action (nn.get(),
                                            action.first.c_str(),
                                            action.second.c_str(),
                                            on_notification_clicked,
                                            nullptr,
                                            nullptr);
        }

        static int next_key = 1;
        const int key = next_key++;
        g_object_set_qdata (G_OBJECT(nn.get()),
                            notification_key_quark(),
                            GINT_TO_POINTER(key));

        m_notifications[key] = { nn, info };
        g_signal_connect (nn.get(), "closed",
                          G_CALLBACK(on_notification_closed), this);

        if (!info.m_show_notification_bubble) {
            post(info);
            return ret;
        }

        GError * error = nullptr;
        if (notify_notification_show(nn.get(), &error))
        {
            ret = key;
        }
        else
        {
            g_critical ("Unable to show notification for '%s': %s",
                        info.m_title.c_str(),
                        error->message);
            g_error_free (error);
            m_notifications.erase(key);
        }

        return ret;
    }

    std::string post(const Builder::Impl& data)
    {
        if (!data.m_post_to_messaging_menu) {
            return "";
        }

        if (!m_messaging_app) {
            return std::string();
        }
        uuid_t message_uuid;
        uuid_generate(message_uuid);

        char uuid_buf[37];
        uuid_unparse(message_uuid, uuid_buf);
        const std::string message_id(uuid_buf);

        // use full icon path name, "calendar-app" does not work with themed icons
        auto icon_file = g_file_new_for_path(calendar_app_icon().c_str());
        // messaging_menu_message_new: will take control of icon object
        GIcon *icon = g_file_icon_new(icon_file);
        g_object_unref(icon_file);

        // check if source exists
        if (!messaging_menu_app_has_source(m_messaging_app.get(), m_app_name.c_str()))
            messaging_menu_app_append_source(m_messaging_app.get(), m_app_name.c_str(), nullptr, "Calendar");

        auto msg = messaging_menu_message_new(message_id.c_str(),
                                              icon,
                                              data.m_title.c_str(),
                                              nullptr,
                                              data.m_body.c_str(),
                                              data.m_start_time * G_USEC_PER_SEC); // secs -> microsecs
        if (msg)
        {
            std::shared_ptr<messaging_menu_data> msg_data(new messaging_menu_data{message_id, data.m_timeout_callback, this});
            m_messaging_messages[message_id] = msg_data;
            g_signal_connect(G_OBJECT(msg), "activate",
                             G_CALLBACK(on_message_activated), msg_data.get());
            messaging_menu_app_append_message(m_messaging_app.get(), msg, m_app_name.c_str(), false);

            // we use that to keep track of messaging, in case of message get cleared from menu
            g_object_set_data_full(G_OBJECT(msg), "destroy-notify", msg_data.get(), on_message_destroyed);
            // keep the message control with message_menu
            g_object_unref(msg);

            return message_id;
        } else {
            g_warning("Fail to create messaging menu message");
        }
        return "";
    }

    void remove (const std::string &key)
    {
        auto it = m_messaging_messages.find(key);
        if (it != m_messaging_messages.end())
        {
            // tell the server to remove message
            messaging_menu_app_remove_message_by_id(m_messaging_app.get(), it->second->msg_id.c_str());
            // message will be remove by on_message_destroyed cb.
        }
    }

    void remove_all ()
    {
        // call remove() on all our keys
        while (!m_messaging_messages.empty())
            remove(m_messaging_messages.begin()->first);
    }

private:

    const std::set<std::string>& server_caps() const
    {
        if (G_UNLIKELY(m_lazy_caps.empty()))
        {
            auto caps_gl = notify_get_server_caps();
            std::string caps_str;
            for(auto l=caps_gl; l!=nullptr; l=l->next)
            {
                m_lazy_caps.insert((const char*)l->data);

                caps_str += (const char*) l->data;;
                if (l->next != nullptr)
                    caps_str += ", ";
            }

            g_debug("%s notify_get_server() returned [%s]", G_STRFUNC, caps_str.c_str());
            g_list_free_full(caps_gl, g_free);
        }

        return m_lazy_caps;
    }

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
        const gpointer gkey = g_object_get_qdata(G_OBJECT(nn), q);
        static_cast<Impl*>(gself)->remove_closed_notification(GPOINTER_TO_INT(gkey));
    }

    static void on_message_activated (MessagingMenuMessage *,
                                      const char *,
                                      GVariant *,
                                      gpointer data)
    {
        auto msg_data =  static_cast<messaging_menu_data*>(data);
        auto it = msg_data->self->m_messaging_messages.find(msg_data->msg_id);
        g_return_if_fail (it != msg_data->self->m_messaging_messages.end());
        const auto& ndata = it->second;

        if (ndata->callback)
            ndata->callback();
    }

    static void on_message_destroyed(gpointer data)
    {
        auto msg_data = static_cast<messaging_menu_data*>(data);
        auto it = msg_data->self->m_messaging_messages.find(msg_data->msg_id);
        if (it != msg_data->self->m_messaging_messages.end())
            msg_data->self->m_messaging_messages.erase(it);
    }

    void remove_closed_notification (int key)
    {
        auto it = m_notifications.find(key);
        g_return_if_fail (it != m_notifications.end());

        const auto& ndata = it->second;
        auto nn = ndata.nn.get();

        if (ndata.data.m_closed_callback)
        {
            std::string action;
            const GQuark q = notification_action_quark();
            const gpointer p = g_object_get_qdata(G_OBJECT(nn), q);
            if (p != nullptr)
                action = static_cast<const char*>(p);

            ndata.data.m_closed_callback (action);
            // empty action means that the notification got timeout
            // post a message on messaging menu
            if (action.empty())
                post(ndata.data);
        }

        m_notifications.erase(it);
    }

    static std::string calendar_app_id()
    {
        #ifdef HAS_LOMIRIAPPLAUNCH
        auto registry = std::make_shared<lomiri::app_launch::Registry>();
        auto app_id = lomiri::app_launch::AppID::discover(registry, "com.lomiri.calendar");

        if (!app_id.empty())
            // Due the use of old API by messaging_menu we need append a extra ".desktop" to the app_id.
            return std::string(app_id) + ".desktop";
        else
            return std::string();

        #else
        return std::string();
        #endif
    }

    static std::string calendar_app_icon()
    {
        auto app_desktop = g_desktop_app_info_new(calendar_app_id().c_str());
        if (app_desktop != nullptr) {
            auto icon_name = g_desktop_app_info_get_string(app_desktop, "Icon");
            g_object_unref(app_desktop);
            if (icon_name) {
                std::string result(icon_name);
                g_free(icon_name);
                return result;
            }
        }
        g_warning("Fail to get calendar icon");
        return std::string();
    }

    /***
    ****
    ***/

    // messaging menu
    std::shared_ptr<MessagingMenuApp> m_messaging_app;
    std::map<std::string, std::shared_ptr<messaging_menu_data> > m_messaging_messages;

    const std::string m_app_name;

    // key-to-data
    std::map<int,notification_data> m_notifications;

    // server capabilities.
    // as the name indicates, don't use this directly: use server_caps() instead
    mutable std::set<std::string> m_lazy_caps;

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
} // namespace ayatana

