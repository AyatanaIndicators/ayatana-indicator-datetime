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

#ifndef UNITY_INDICATOR_NOTIFICATIONS_NOTIFICATIONS_H
#define UNITY_INDICATOR_NOTIFICATIONS_NOTIFICATIONS_H

#include <chrono>
#include <functional>
#include <memory>
#include <string>

namespace unity {
namespace indicator {
namespace notifications {

class Engine;

/**
 * Helper class for showing notifications.
 *
 * Populate the builder, then pass it to Engine::show().
 * 
 * @see Engine::show(Builder)
 */
class Builder
{
public:
    Builder();
    ~Builder();

    void set_title (const std::string& title);

    void set_body (const std::string& body);

    void set_icon_name (const std::string& icon_name);

    /* Set an interval, after which the notification will automatically
       be closed. If not set, the notification server's default timeout
       is used. */
    void set_timeout (const std::chrono::seconds& duration);

    /* Add a notification hint.
       These keys may be dependent on the notification server. */
    void add_hint (const std::string& name);
    static constexpr char const * HINT_SNAP {"x-canonical-snap-decisions"};
    static constexpr char const * HINT_TINT {"x-canonical-private-button-tint"};

    /* Add an action button.
       This may fail if the Engine doesn't support actions.
       @see Engine::supports_actions() */
    void add_action (const std::string& action, const std::string& label);

    /** Sets the closed callback. This will be called exactly once. */
    void set_closed_callback (std::function<void(const std::string& action)>);

private:
    friend class Engine;
    class Impl;
    std::unique_ptr<Impl> impl;
};

/**
 * Manages Notifications and the connection to the notification server.
 *
 * When this class is destroyed, any remaining notifications it created
 * will be closed and their closed() callbacks will be invoked.
 */
class Engine
{
public:
    Engine(const std::string& app_name);
    ~Engine();

    /** @see Builder::set_action() */
    bool supports_actions() const;

    /** Show a notification.
        @return zero on failure, or a key that can be passed to close() */
    int show(const Builder& builder);

    /** Close a notification.
        @param key the int returned by show() */
    void close(int key);

    /** Close all remaining notifications. */
    void close_all();

    const std::string& app_name() const;
    
private:
    class Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace notifications
} // namespace indicator
} // namespace unity

#endif // UNITY_INDICATOR_NOTIFICATIONS_NOTIFICATIONS_H
