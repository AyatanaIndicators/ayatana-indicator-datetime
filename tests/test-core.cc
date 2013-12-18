
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

#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>

#include <langinfo.h>
#include <locale.h>

#include <glib/gi18n.h>

#include <core/connection.h>
#include <core/signal.h>
#include <core/property.h>

#include "glib-fixture.h"

/***
****
***/

class CoreFixture: public GlibFixture
{
  private:

    typedef GlibFixture super;

  protected:

    virtual void SetUp ()
    {
      super::SetUp ();
    }

    virtual void TearDown ()
    {
      super::TearDown ();
    }
};

namespace
{
struct EventLoop
{
    typedef std::function<void()> Handler;

    void stop()
    {
        stop_requested = true;
    }

    void run()
    {
        while (!stop_requested)
        {
            std::unique_lock<std::mutex> ul(guard);
            wait_condition.wait_for(
                        ul,
                        std::chrono::milliseconds{500},
                        [this]() { return handlers.size() > 0; });

            std::cerr << "handlers.size() is " << handlers.size() << std::endl;
            while (handlers.size() > 0)
            {
                std::cerr << "gaba begin" << std::endl;
                handlers.front()();
                std::cerr << "gaba end" << std::endl;
                handlers.pop();
            }
        }
    }

    void dispatch(const Handler& h)
    {
std::cerr << "in dispatch" << std::endl;
        std::lock_guard<std::mutex> lg(guard);
        handlers.push(h);
    }

    bool stop_requested = false;
    std::queue<Handler> handlers;
    std::mutex guard;
    std::condition_variable wait_condition;
};
}


TEST_F (CoreFixture, HelloWorld)
{
    // We instantiate an event loop and run it on a different thread than the main one.
    EventLoop dispatcher;
    std::thread dispatcher_thread{[&dispatcher]() { dispatcher.run(); }};
    std::thread::id dispatcher_thread_id = dispatcher_thread.get_id();

    // The signal that we want to dispatch via the event loop.
    core::Signal<int, double> s;

    static const int expected_invocation_count = 10000;

    // Setup the connection. For each invocation we check that the id of the
    // thread the handler is being called upon equals the thread that the
    // event loop is running upon.
    auto connection = s.connect(
                [&dispatcher, dispatcher_thread_id](int value, double d)
                {
                    std::cerr << "this is the lambda" << std::endl;
                    EXPECT_EQ(dispatcher_thread_id,
                              std::this_thread::get_id());

                    std::cout << d << std::endl;

                    if (value == expected_invocation_count)
                        dispatcher.stop();
                });

    // Route the connection via the dispatcher
    connection.dispatch_via(
                std::bind(
                    &EventLoop::dispatch,
                    std::ref(dispatcher),
                    std::placeholders::_1));

    // Invoke the signal from the main thread.
    for (unsigned int i = 1; i <= expected_invocation_count; i++)
        s(i, 42.);

    if (dispatcher_thread.joinable())
        dispatcher_thread.join();
}
