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
 * RAII wrapper for track reference counting
 *
 * Provides automatic reference counting for track objects, preventing
 * leaks and use-after-free bugs. Move-only to enforce single ownership
 * semantics.
 */

#pragma once

#include "../player/track.h"

namespace sc {

class TrackRef {
    Track* ptr = nullptr;

public:
    // Construct empty reference
    TrackRef() = default;

    // Take ownership of a track (assumes refcount already incremented)
    explicit TrackRef(Track* t) : ptr(t) {}

    // No copy - tracks have reference semantics
    TrackRef(const TrackRef&) = delete;
    TrackRef& operator=(const TrackRef&) = delete;

    // Move constructor
    TrackRef(TrackRef&& other) noexcept : ptr(other.ptr) {
        other.ptr = nullptr;
    }

    // Move assignment
    TrackRef& operator=(TrackRef&& other) noexcept {
        if (this != &other) {
            if (ptr) {
                track_release(ptr);
            }
            ptr = other.ptr;
            other.ptr = nullptr;
        }
        return *this;
    }

    // Destructor releases the track
    ~TrackRef() {
        if (ptr) {
            track_release(ptr);
        }
    }

    // Access the underlying track
    Track* get() const { return ptr; }

    // Arrow operator for convenience
    Track* operator->() const { return ptr; }

    // Dereference operator
    Track& operator*() const { return *ptr; }

    // Boolean conversion for null checks
    explicit operator bool() const { return ptr != nullptr; }

    // Release ownership without decrementing refcount
    // Used when transferring to C code that takes ownership
    Track* release() {
        Track* t = ptr;
        ptr = nullptr;
        return t;
    }

    // Reset with a new track (releases old one if any)
    void reset(Track* t = nullptr) {
        if (ptr) {
            track_release(ptr);
        }
        ptr = t;
    }
};

} // namespace sc
