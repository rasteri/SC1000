/*
 * Copyright (C) 2018 Mark Hills <mark@xwax.org>
 * Copyright (C) 2019 Andrew Tait <rasteri@gmail.com>
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

#pragma once

#include <cstddef>
#include <sys/poll.h>
#include <sys/types.h>

struct Deck;
struct Rt;

/*
 * Controller - abstract base class for input controllers
 *
 * A controller is a MIDI controller or HID device used to control the program.
 * Derived classes implement the virtual methods for their specific hardware.
 */
class Controller {
public:
    Controller() = default;
    virtual ~Controller() = default;

    // Non-copyable (controllers own hardware resources)
    Controller(const Controller&) = delete;
    Controller& operator=(const Controller&) = delete;

    // Movable
    Controller(Controller&&) = default;
    Controller& operator=(Controller&&) = default;

    /*
     * Add a deck to this controller
     * Return: 0 on success, -1 if deck could not be added
     */
    virtual int add_deck(struct Deck* d) = 0;

    /*
     * Get file descriptors for poll()
     * Return: number of pollfd entries filled, or -1 on error
     */
    virtual ssize_t pollfds(struct pollfd* pe, size_t z) = 0;

    /*
     * Handle realtime events (called from realtime thread)
     * Return: 0 on success, -1 on error
     */
    virtual int realtime() = 0;

    /*
     * Clean up resources
     */
    virtual void clear() = 0;

    // Fault state
    bool has_fault() const { return fault_; }
    void set_fault() { fault_ = true; }

protected:
    bool fault_ = false;
};

// Helper functions that work with Controller*
void controller_add_deck(Controller* c, struct Deck* d);
ssize_t controller_pollfds(Controller* c, struct pollfd* pe, size_t z);
void controller_handle(Controller* c);
void controller_clear(Controller* c);
