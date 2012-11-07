/*
Copyright 2012 Canonical Ltd.

Authors:
    Charles Kerr <charles.kerr@canonical.com>

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

#include <gtest/gtest.h>

#include <glib-object.h>

/***
****
***/

namespace
{
  void ensure_glib_initialized ()
  {
    static bool initialized = false;

    if (G_UNLIKELY(!initialized))
    {
      initialized = true;
      g_type_init();
      g_setenv ("GSETTINGS_SCHEMA_DIR", SCHEMA_DIR, TRUE);
    }
  }
}

/***
****
***/

class IndicatorTest : public ::testing::Test
{
  private:

    guint log_handler_id;

    int log_count_ipower_actual;

    static void log_count_func (const gchar *log_domain,
                                GLogLevelFlags log_level,
                                const gchar *message,
                                gpointer user_data)
    {
      reinterpret_cast<IndicatorTest*>(user_data)->log_count_ipower_actual++;
    }

  protected:

    int log_count_ipower_expected;

  protected:

    virtual void SetUp()
    {
      const GLogLevelFlags flags = GLogLevelFlags(G_LOG_LEVEL_CRITICAL|G_LOG_LEVEL_WARNING);
      log_handler_id = g_log_set_handler ("Indicator-Power", flags, log_count_func, this);
      log_count_ipower_expected = 0;
      log_count_ipower_actual = 0;

      ensure_glib_initialized ();
    }

    virtual void TearDown()
    {
      ASSERT_EQ (log_count_ipower_expected, log_count_ipower_actual);
      g_log_remove_handler ("Indicator-Power", log_handler_id);
    }
};

/***
****
***/

TEST_F(IndicatorTest, HelloWorld)
{
  ASSERT_TRUE (TRUE);
}
