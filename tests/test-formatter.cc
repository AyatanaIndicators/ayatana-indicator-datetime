
/*
 * Copyright 2013 Canonical Ltd.
 *
 * Authors:
 *   Charles Kerr <charles.kerr@canonical.com>
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
 */

#include <langinfo.h>
#include <locale.h>

#include <glib/gi18n.h>

#include "utils.h"

#include "glib-fixture.h"

/***
****
***/

class FormatterFixture: public GlibFixture
{
  private:

    typedef GlibFixture super;
    gchar * original_locale = nullptr;

  protected:

    virtual void SetUp ()
    {
      super::SetUp ();
  
      original_locale = g_strdup (setlocale (LC_TIME, NULL));
    }

    virtual void TearDown ()
    {
      setlocale (LC_TIME, original_locale);
      g_clear_pointer (&original_locale, g_free);

      super::TearDown ();
    }

    bool SetLocale (const char * expected_locale, const char * name)
    {
      setlocale (LC_TIME, expected_locale);
      const char * actual_locale = setlocale (LC_TIME, NULL);
      if (!g_strcmp0 (expected_locale, actual_locale))
        {
          return true;
        }
      else
        {
          g_warning ("Unable to set locale to %s; skipping %s locale tests.", expected_locale, name);
          return false;
        }
    }

    inline bool Set24hLocale () { return SetLocale ("C",          "24h"); }
    inline bool Set12hLocale () { return SetLocale ("en_US.utf8", "12h"); }
};


/**
 * Test the phone header format
 */
TEST_F (FormatterFixture, TestPhoneHeader)
{
  // test the default value in a 24h locale
  if (Set24hLocale ())
    {
      const gchar * format = get_terse_header_time_format_string ();
      ASSERT_NE (nullptr, format);
      ASSERT_STREQ ("%H:%M", format);
    }

  // test the default value in a 12h locale
  if (Set12hLocale ())
    {
      const gchar * format = get_terse_header_time_format_string ();
      ASSERT_NE (nullptr, format);
      ASSERT_STREQ ("%l:%M %p", format);
    }
}
