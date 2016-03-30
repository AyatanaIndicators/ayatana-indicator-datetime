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

#ifndef INDICATOR_DATETIME_MYSELF_H
#define INDICATOR_DATETIME_MYSELF_H

#include <libaccounts-glib.h>

#include <core/property.h>

#include <string>
#include <vector>
#include <memory.h>

namespace ayatana {
namespace indicator {
namespace datetime {

class Myself
{
public:
     Myself();

     core::Property<std::vector<std::string>>& emails()
     {
         return m_emails;
     }

     bool isMyEmail(const std::string &email);

private:
     std::shared_ptr<AgManager> m_accounts_manager;
     core::Property<std::vector<std::string> > m_emails;

     static void on_accounts_changed(AgManager*, guint, Myself*);
     void reloadEmails();

};


} // namespace datetime
} // namespace indicator
} // namespace ayatana

#endif // INDICATOR_DATETIME_MYSELF_H
