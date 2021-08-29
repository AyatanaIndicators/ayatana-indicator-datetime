/*
 * Copyright 2016 Canonical Ltd.
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
 *   Renato Araujo Oliveira Filho <renato.filho@canonical.com>
 */

#include "datetime/myself.h"

#include <libaccounts-glib.h>
#include <libaccounts-glib/ag-account.h>

#include <algorithm>

namespace ayatana {
namespace indicator {
namespace datetime {

Myself::Myself()
    : m_accounts_manager(ag_manager_new(), g_object_unref)
{
    reloadEmails();
    g_object_connect(m_accounts_manager.get(),
                     "signal::account-created", on_accounts_changed, this,
                     "signal::account-deleted", on_accounts_changed, this,
                     "signal::account-updated", on_accounts_changed, this,
                     nullptr);
}

bool Myself::isMyEmail(const std::string &email)
{
    auto emails = m_emails.get();
    return (std::find(emails.begin(), emails.end(), email) != emails.end());
}

void Myself::on_accounts_changed(AgManager *, guint, Myself *self)
{
    self->reloadEmails();
}

void Myself::reloadEmails()
{
    std::vector<std::string> emails;

    auto manager = m_accounts_manager.get();
    auto ids = ag_manager_list(manager);
    for (auto l=ids; l!=nullptr; l=l->next)
    {
        auto acc = ag_manager_get_account(manager, GPOINTER_TO_UINT(l->data));
        auto account_name = ag_account_get_display_name(acc);
        emails.push_back(account_name);
    }
    ag_manager_list_free(ids);

    m_emails.set(emails);
}

} // namespace datetime
} // namespace indicator
} // namespace ayatana

