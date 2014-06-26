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

#include <datetime/dbus-shared.h>
#include <datetime/exporter.h>

#include <glib/gi18n.h>
#include <gio/gio.h>

namespace unity {
namespace indicator {
namespace datetime {

/***
****
***/

class Exporter::Impl
{
public:

    Impl (Exporter* owner):
        m_owner(owner)
    {
    }

    ~Impl()
    {
        if (m_bus != nullptr)
        {
            for(auto& id : m_exported_menu_ids)
                g_dbus_connection_unexport_menu_model(m_bus, id);

            if (m_exported_actions_id)
                g_dbus_connection_unexport_action_group(m_bus, m_exported_actions_id);
        }

        if (m_own_id)
            g_bus_unown_name(m_own_id);

        g_clear_object(&m_bus);
    }

    core::Signal<> name_lost;

    void publish(const std::shared_ptr<Actions>& actions,
                 const std::vector<std::shared_ptr<Menu>>& menus)
    {
        m_actions = actions;
        m_menus = menus;
        m_own_id = g_bus_own_name(G_BUS_TYPE_SESSION,
                                  BUS_NAME,
                                  G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT,
                                  on_bus_acquired,
                                  nullptr,
                                  on_name_lost,
                                  this,
                                  nullptr);
    }

private:

    static void on_bus_acquired(GDBusConnection* connection,
                                const gchar* name,
                                gpointer gthis)
    {
        g_debug("bus acquired: %s", name);
        static_cast<Impl*>(gthis)->on_bus_acquired(connection, name);
    }

    void on_bus_acquired(GDBusConnection* bus, const gchar* /*name*/)
    {
        m_bus = static_cast<GDBusConnection*>(g_object_ref(G_OBJECT(bus)));

        // export the actions
        GError * error = nullptr;
        const auto id = g_dbus_connection_export_action_group(m_bus,
                                                              BUS_PATH,
                                                              m_actions->action_group(),
                                                              &error);
        if (id)
        {
            m_exported_actions_id = id;
        }
        else
        {
            g_warning("cannot export action group: %s", error->message);
            g_clear_error(&error);
        }

        // export the menus
        for(auto& menu : m_menus)
        {
            const auto path = std::string(BUS_PATH) + "/" + menu->name();
            const auto id = g_dbus_connection_export_menu_model(m_bus, path.c_str(), menu->menu_model(), &error);
            if (id)
            {
                m_exported_menu_ids.insert(id);
            }
            else
            {
                if (error != nullptr)
                    g_warning("cannot export %s menu: %s", menu->name().c_str(), error->message);
                g_clear_error(&error);
            }
        }
    }

    /***
    ****
    ***/

    static void on_name_lost(GDBusConnection*, const gchar* name, gpointer gthis)
    {
        g_debug("name lost: %s", name);
        static_cast<Impl*>(gthis)->name_lost();
    }

    /***
    ****
    ***/

    Exporter const * m_owner;
    std::set<guint> m_exported_menu_ids;
    guint m_own_id = 0;
    guint m_exported_actions_id = 0;
    GDBusConnection* m_bus = nullptr;
    std::shared_ptr<Actions> m_actions;
    std::vector<std::shared_ptr<Menu>> m_menus;
};


/***
****
***/

Exporter::Exporter():
    p(new Impl(this))
{
}


Exporter::~Exporter()
{
}

core::Signal<>& Exporter::name_lost()
{
    return p->name_lost;
}

void Exporter::publish(const std::shared_ptr<Actions>& actions,
                       const std::vector<std::shared_ptr<Menu>>& menus)
{
    p->publish(actions, menus);
}

/***
****
***/

} // namespace datetime
} // namespace indicator
} // namespace unity

