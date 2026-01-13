/*
 * Copyright (C) 2024-2026 Niklas Klügel <lodsb@lodsb.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 *
 * You should have received a copy of the GNU General Public License
 * version 2 along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 */


/*
 * Modern C++ Event/Signal implementation
 *
 * Replaces the old C-style observer pattern that used intrusive linked lists.
 * Uses std::function for type-safe callbacks and std::vector for storage.
 *
 * Usage:
 *   Event<int> on_value_changed;
 *
 *   // Connect a callback
 *   auto id = on_value_changed.connect([](int value) {
 *       printf("Value changed to %d\n", value);
 *   });
 *
 *   // Emit the event
 *   on_value_changed.emit(42);
 *
 *   // Disconnect when done
 *   on_value_changed.disconnect(id);
 */

#pragma once

#include <functional>
#include <vector>
#include <cstdint>

namespace sc {

// Connection ID for tracking and disconnecting callbacks
using ConnectionId = uint32_t;

/*
 * Event class - emits notifications to connected callbacks
 *
 * Template parameter Args... are the types passed to callbacks when emitting.
 * Use Event<> for events with no data.
 */
template<typename... Args>
class Event {
public:
    using Callback = std::function<void(Args...)>;

    Event() = default;
    ~Event() = default;

    // Non-copyable (connections would be duplicated)
    Event(const Event&) = delete;
    Event& operator=(const Event&) = delete;

    // Movable
    Event(Event&&) = default;
    Event& operator=(Event&&) = default;

    /*
     * Connect a callback to this signal
     * Returns a ConnectionId that can be used to disconnect later
     */
    ConnectionId connect(Callback callback) {
        ConnectionId id = next_id_++;
        connections_.push_back({id, std::move(callback)});
        return id;
    }

    /*
     * Disconnect a callback by its ConnectionId
     * Returns true if the connection was found and removed
     */
    bool disconnect(ConnectionId id) {
        for (auto it = connections_.begin(); it != connections_.end(); ++it) {
            if (it->id == id) {
                connections_.erase(it);
                return true;
            }
        }
        return false;
    }

    /*
     * Disconnect all callbacks
     */
    void disconnect_all() {
        connections_.clear();
    }

    /*
     * Emit the signal, calling all connected callbacks
     */
    void emit(Args... args) {
        // Copy the connections in case a callback modifies the list
        auto connections_copy = connections_;
        for (const auto& conn : connections_copy) {
            conn.callback(args...);
        }
    }

    /*
     * Check if any callbacks are connected
     */
    bool has_connections() const {
        return !connections_.empty();
    }

    /*
     * Get the number of connected callbacks
     */
    size_t connection_count() const {
        return connections_.size();
    }

private:
    struct Connection {
        ConnectionId id;
        Callback callback;
    };

    std::vector<Connection> connections_;
    ConnectionId next_id_ = 1;
};

/*
 * ScopedConnection - RAII wrapper that auto-disconnects on destruction
 *
 * Usage:
 *   {
 *       ScopedConnection conn(event, [](int x) { ... });
 *       // callback is active
 *   }
 *   // callback is automatically disconnected
 */
template<typename... Args>
class ScopedConnection {
public:
    ScopedConnection(Event<Args...>& event, typename Event<Args...>::Callback callback)
        : event_(&event)
        , id_(event.connect(std::move(callback)))
    {}

    ~ScopedConnection() {
        if (event_) {
            event_->disconnect(id_);
        }
    }

    // Non-copyable
    ScopedConnection(const ScopedConnection&) = delete;
    ScopedConnection& operator=(const ScopedConnection&) = delete;

    // Movable
    ScopedConnection(ScopedConnection&& other) noexcept
        : event_(other.event_)
        , id_(other.id_)
    {
        other.event_ = nullptr;
    }

    ScopedConnection& operator=(ScopedConnection&& other) noexcept {
        if (this != &other) {
            if (event_) {
                event_->disconnect(id_);
            }
            event_ = other.event_;
            id_ = other.id_;
            other.event_ = nullptr;
        }
        return *this;
    }

    ConnectionId id() const { return id_; }

private:
    Event<Args...>* event_;
    ConnectionId id_;
};

} // namespace sc
