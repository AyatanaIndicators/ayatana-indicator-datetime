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

#pragma once

#include <chrono>
#include <functional> // std::function
#include <map>
#include <memory> // std::shared_ptr

#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>

#include <gtest/gtest.h>

#include <locale.h> // setlocale()

class GlibFixture : public ::testing::Test
{
  public:

    virtual ~GlibFixture() =default;

  protected:

    virtual void SetUp() override
    {
      setlocale(LC_ALL, "C.UTF-8");

      loop = g_main_loop_new(nullptr, false);

      // only use local, temporary settings
      g_assert(g_setenv("GSETTINGS_SCHEMA_DIR", SCHEMA_DIR, true));
      g_assert(g_setenv("GSETTINGS_BACKEND", "memory", true));
      g_debug("SCHEMA_DIR is %s", SCHEMA_DIR);

      // fail on unexpected messages from this domain
      g_log_set_fatal_mask(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING);

      g_unsetenv("DISPLAY");

    }

    virtual void TearDown() override
    {
      g_test_assert_expected_messages ();

      g_clear_pointer(&loop, g_main_loop_unref);
    }

    void expectLogMessage (const gchar *domain, GLogLevelFlags level, const gchar *pattern)
    {
      g_test_expect_message (domain, level, pattern);
    }

  private:

    static gboolean
    wait_for_signal__timeout(gpointer name)
    {
      g_error("%s: timed out waiting for signal '%s'", G_STRLOC, (char*)name);
      return G_SOURCE_REMOVE;
    }

    static gboolean
    wait_msec__timeout(gpointer loop)
    {
      g_main_loop_quit(static_cast<GMainLoop*>(loop));
      return G_SOURCE_CONTINUE;
    }

  protected:

    /* convenience func to loop while waiting for a GObject's signal */
    void wait_for_signal(gpointer o, const gchar * signal, const int timeout_seconds=5)
    {
      // wait for the signal or for timeout, whichever comes first
      const auto handler_id = g_signal_connect_swapped(o, signal,
                                                       G_CALLBACK(g_main_loop_quit),
                                                       loop);
      const auto timeout_id = g_timeout_add_seconds(timeout_seconds,
                                                    wait_for_signal__timeout,
                                                    loop);
      g_main_loop_run(loop);
      g_source_remove(timeout_id);
      g_signal_handler_disconnect(o, handler_id);
    }

    /* convenience func to loop for N msec */
    void wait_msec(int msec=50)
    {
      const auto id = g_timeout_add(msec, wait_msec__timeout, loop);
      g_main_loop_run(loop);
      g_source_remove(id);
    }

    bool wait_for(std::function<bool()> test_function, guint timeout_msec=1000)
    {
      auto timer = std::shared_ptr<GTimer>(g_timer_new(), [](GTimer* t){g_timer_destroy(t);});
      const auto timeout_sec = timeout_msec / 1000.0;
      for (;;) {
        if (test_function())
          return true;
        //g_message("%f ... %f", g_timer_elapsed(timer.get(), nullptr), timeout_sec);
        if (g_timer_elapsed(timer.get(), nullptr) >= timeout_sec)
          return false;
        wait_msec();
      }
    }

    bool wait_for_name_owned(GDBusConnection* connection,
                             const gchar* name,
                             guint timeout_msec=1000,
                             GBusNameWatcherFlags flags=G_BUS_NAME_WATCHER_FLAGS_AUTO_START)
    {
      struct Data {
        GMainLoop* loop = nullptr;
        bool owned = false;
      };
      Data data;

      auto on_name_appeared = [](GDBusConnection* /*connection*/,
                                 const gchar* /*name_*/,
                                 const gchar* name_owner,
                                 gpointer gdata)
      {
        if (name_owner == nullptr)
          return;
        auto tmp = static_cast<Data*>(gdata);
        tmp->owned = true;
        g_main_loop_quit(tmp->loop);
      };

      const auto timeout_id = g_timeout_add(timeout_msec, wait_msec__timeout, loop);
      data.loop = loop;
      const auto watch_id = g_bus_watch_name_on_connection(connection,
                                                           name,
                                                           flags,
                                                           on_name_appeared,
                                                           nullptr, /* name_vanished */
                                                           &data,
                                                           nullptr); /* user_data_free_func */
      g_main_loop_run(loop);

      g_bus_unwatch_name(watch_id);
      g_source_remove(timeout_id);

      return data.owned;
    }

    void EXPECT_NAME_OWNED_EVENTUALLY(GDBusConnection* connection,
                                      const gchar* name,
                                      guint timeout_msec=1000,
                                      GBusNameWatcherFlags flags=G_BUS_NAME_WATCHER_FLAGS_AUTO_START)
    {
      EXPECT_TRUE(wait_for_name_owned(connection, name, timeout_msec, flags)) << "name: " << name;
    }

    void EXPECT_NAME_NOT_OWNED_EVENTUALLY(GDBusConnection* connection,
                                          const gchar* name,
                                          guint timeout_msec=1000,
                                          GBusNameWatcherFlags flags=G_BUS_NAME_WATCHER_FLAGS_AUTO_START)
    {
      EXPECT_FALSE(wait_for_name_owned(connection, name, timeout_msec, flags)) << "name: " << name;
    }

    void ASSERT_NAME_OWNED_EVENTUALLY(GDBusConnection* connection,
                                      const gchar* name,
                                      guint timeout_msec=1000,
                                      GBusNameWatcherFlags flags=G_BUS_NAME_WATCHER_FLAGS_AUTO_START)
    {
      ASSERT_TRUE(wait_for_name_owned(connection, name, timeout_msec, flags)) << "name: " << name;
    }

    void ASSERT_NAME_NOT_OWNED_EVENTUALLY(GDBusConnection* connection,
                                          const gchar* name,
                                          guint timeout_msec=1000,
                                          GBusNameWatcherFlags flags=G_BUS_NAME_WATCHER_FLAGS_AUTO_START)
    {
      ASSERT_FALSE(wait_for_name_owned(connection, name, timeout_msec, flags)) << "name: " << name;
    }

    using source_func = std::function<gboolean()>;

    guint idle_add(source_func&& func)
    {
      return g_idle_add_full(
        G_PRIORITY_DEFAULT_IDLE,
        [](gpointer gf){return (*static_cast<source_func*>(gf))();},
        new std::function<gboolean()>(func),
        [](gpointer gf){delete static_cast<source_func*>(gf);}
      );
    }

    guint timeout_add(source_func&& func, std::chrono::milliseconds msec)
    {
      return g_timeout_add_full(
        G_PRIORITY_DEFAULT,
        msec.count(),
        [](gpointer gf){return (*static_cast<source_func*>(gf))();},
        new std::function<gboolean()>(func),
        [](gpointer gf){delete static_cast<source_func*>(gf);}
      );
    }

    GMainLoop* loop {};
};

