/* -*- Mode: C; coding: utf-8; indent-tabs-mode: nil; tab-width: 2 -*-

A dialog for setting time and date preferences.

Copyright 2010 Canonical Ltd.

Authors:
    Michael Terry <michael.terry@canonical.com>

This program is free software: you can redistribute it and/or modify it 
under the terms of the GNU General Public License version 3, as published 
by the Free Software Foundation.

This program is distributed in the hope that it will be useful, but 
WITHOUT ANY WARRANTY; without even the implied warranties of 
MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR 
PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along 
with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <locale.h>
#include <langinfo.h>
#include <string.h>
#include "utils.h"

/* Check the system locale setting to see if the format is 24-hour
   time or 12-hour time */
gboolean
is_locale_12h (void)
{
	static const char *formats_24h[] = {"%H", "%R", "%T", "%OH", "%k", NULL};
	const char *t_fmt = nl_langinfo (T_FMT);
	int i;

	for (i = 0; formats_24h[i]; ++i) {
		if (strstr (t_fmt, formats_24h[i])) {
			return FALSE;
		}
	}

	return TRUE;
}

