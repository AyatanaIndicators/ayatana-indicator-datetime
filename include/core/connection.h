/*
 * Copyright © 2013 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Thomas Voß <thomas.voss@canonical.com>
 */
#ifndef COM_UBUNTU_CONNECTION_H_
#define COM_UBUNTU_CONNECTION_H_

#include <functional>
#include <memory>
#include <mutex>

namespace core
{
/**
 * @brief The Connection class models a signal-slot connection.
 */
class Connection
{
public:    
    typedef std::function<void(const std::function<void()>&)> Dispatcher;

    /**
     * @brief Checks if this instance corresponds to an active signal-slot connection.
     * @return true iff the instance corresponds to an active signal-slot connection.
     */
    inline bool is_connected() const
    {
        return (d->disconnector ? true : false);
    }

    /**
     * @brief End a signal-slot connection.
     */
    inline void disconnect()
    {
        d->disconnect();
    }

    /**
     * @brief Installs a dispatcher for this signal-slot connection.
     * @param dispatcher The dispatcher to be used for signal emissions.
     */
    inline void dispatch_via(const Dispatcher& dispatcher)
    {
        if (d->dispatcher_installer)
            d->dispatcher_installer(dispatcher);
    }

private:
    typedef std::function<void()> Disconnector;
    typedef std::function<void(const Dispatcher&)> DispatcherInstaller;

    template<typename ... Arguments> friend class Signal;

    inline Connection(const Disconnector& disconnector,
                      const DispatcherInstaller& installer)
        : d(std::make_shared<Private>(disconnector, installer))
    {
    }

    inline void reset()
    {
        d->reset();
    }

    struct Private
    {
        Private(const Connection::Disconnector& disconnector_,
                const Connection::DispatcherInstaller& dispatcher_installer_)
            : disconnector(disconnector_),
              dispatcher_installer(dispatcher_installer_)
        {
        }

        inline void reset()
        {
            std::lock_guard<std::mutex> lg(guard);
            reset_locked();
        }

        inline void reset_locked()
        {
            static const Connection::Disconnector empty_disconnector{};
            static const Connection::DispatcherInstaller empty_dispatcher_installer{};

            disconnector = empty_disconnector;
            dispatcher_installer = empty_dispatcher_installer;
        }

        inline void disconnect()
        {
            static const Connection::Disconnector empty_disconnector{};

            std::lock_guard<std::mutex> lg(guard);

            if (disconnector)
                disconnector();

            reset_locked();
        }

        std::mutex guard;
        Connection::Disconnector disconnector;
        Connection::DispatcherInstaller dispatcher_installer;
    };

    // The whole class is implicitly shared and we thus forward our complete
    // shared state to a private structure that is lifetime-managed by a shared_ptr.
    std::shared_ptr<Private> d;
};

/**
 * @brief Scoped helper class to map signal-slot connection mgmt. to RAII.
 */
class ScopedConnection
{
public:
    /**
     * @brief Constructs an instance for an existing signal-slot connection.
     * @param c The existing signal-slot connection.
     */
    inline ScopedConnection(const Connection& c) : connection(c)
    {
    }

    ScopedConnection(const ScopedConnection&) = delete;

    /**
     * @brief Disconnects the signal-slot connection.
     */
    inline ~ScopedConnection() noexcept(true)
    {
        try
        {
            connection.disconnect();
        } catch(...)
        {
        }
    }

    ScopedConnection& operator=(const ScopedConnection&) = delete;
    bool operator==(const ScopedConnection&) = delete;

private:
    Connection connection;
};
}

#endif // COM_UBUNTU_CONNECTION_H_
