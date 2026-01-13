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

// Hardware Abstraction Layer for input devices
// Abstract interface allows different hardware platforms (SC1000, motorized platter, etc.)

#pragma once

#include <memory>

struct Sc1000;

namespace sc {
namespace platform {

//
// HardwareInput - Abstract interface for hardware input platforms
//
// Implement this for each hardware platform (SC1000, motorized platter, desktop test, etc.)
// The input thread interacts only with this interface, not hardware-specific details.
//
class HardwareInput {
public:
    virtual ~HardwareInput() = default;

    // Lifecycle
    virtual bool init(Sc1000* engine) = 0;

    // Called every input loop iteration
    virtual void poll(Sc1000* engine) = 0;

    // Stats/debugging (called once per second)
    virtual void log_stats(Sc1000* engine) = 0;

    // Capability queries (override to advertise features)
    virtual bool has_motor_control() const { return false; }
    virtual bool has_force_feedback() const { return false; }

    // Motor control interface (override if has_motor_control() returns true)
    virtual void set_motor_speed(double speed) { (void)speed; }
    virtual void set_motor_brake(bool brake) { (void)brake; }
};

//
// Factory function - creates appropriate hardware for the platform
//
// Currently returns SC1000Hardware. Future: could auto-detect or use config.
//
std::unique_ptr<HardwareInput> create_hardware();

} // namespace platform
} // namespace sc
