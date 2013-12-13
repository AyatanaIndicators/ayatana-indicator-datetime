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

#include <map>

#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>

#include <gtest/gtest.h>

class GlibFixture : public ::testing::Test
{
  private:

    GLogFunc realLogHandler;

  protected:

    std::map<GLogLevelFlags,int> logCounts;

    void testLogCount (GLogLevelFlags log_level, int expected G_GNUC_UNUSED)
    {
      ASSERT_EQ (expected, logCounts[log_level]);

      logCounts.erase (log_level);
    }

  private:

    static void default_log_handler (const gchar    * log_domain G_GNUC_UNUSED,
                                     GLogLevelFlags   log_level,
                                     const gchar    * message    G_GNUC_UNUSED,
                                     gpointer         self)
    {
      static_cast<GlibFixture*>(self)->logCounts[log_level]++;
    }

  protected:

    virtual void SetUp ()
    {
      loop = g_main_loop_new (NULL, FALSE);

      g_log_set_default_handler (default_log_handler, this);

      // only use local, temporary settings
      g_setenv ("GSETTINGS_SCHEMA_DIR", SCHEMA_DIR, TRUE);
      g_setenv ("GSETTINGS_BACKEND", "memory", TRUE);
      g_debug ("SCHEMA_DIR is %s", SCHEMA_DIR);
    }

    virtual void TearDown()
    {
      // confirm there aren't any unexpected log messages
      ASSERT_EQ (0, logCounts[G_LOG_LEVEL_ERROR]);
      ASSERT_EQ (0, logCounts[G_LOG_LEVEL_CRITICAL]);
      ASSERT_EQ (0, logCounts[G_LOG_LEVEL_WARNING]);
      ASSERT_EQ (0, logCounts[G_LOG_LEVEL_MESSAGE]);
      ASSERT_EQ (0, logCounts[G_LOG_LEVEL_INFO]);

      // revert to glib's log handler
      g_log_set_default_handler (realLogHandler, this);

      g_clear_pointer (&loop, g_main_loop_unref);
    }

  private:

    static gboolean
    wait_for_signal__timeout (gpointer name)
    {
      g_error ("%s: timed out waiting for signal '%s'", G_STRLOC, (char*)name);
      return G_SOURCE_REMOVE;
    }

  protected:

    /* convenience func to loop while waiting for a GObject's signal */
    void wait_for_signal (gpointer o, const gchar * signal, const int timeout_seconds=5)
    {
      // wait for the signal or for timeout, whichever comes first
      guint handler_id = g_signal_connect_swapped (o, signal,
                                                   G_CALLBACK(g_main_loop_quit),
                                                   loop);
      gulong timeout_id = g_timeout_add_seconds (timeout_seconds,
                                                 wait_for_signal__timeout,
                                                 loop);
      g_main_loop_run (loop);
      g_source_remove (timeout_id);
      g_signal_handler_disconnect (o, handler_id);
    }

    /* convenience func to loop for N msec */
    void wait_msec (int msec=50)
    {
      guint id = g_timeout_add (msec, (GSourceFunc)g_main_loop_quit, loop);
      g_main_loop_run (loop);
      g_source_remove (id);
    }

  GMainLoop * loop;
};
