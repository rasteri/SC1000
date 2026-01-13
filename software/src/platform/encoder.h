/*
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


// AS5600 magnetic rotary encoder interface
#pragma once

#include <cstdint>

namespace sc {
namespace platform {

struct EncoderState {
    int i2c_fd = -1;
    bool present = false;
};

// Initialize rotary encoder on I2C bus
bool encoder_init(EncoderState* state);

// Read current angle (12-bit, 0-4095)
uint16_t encoder_read_angle(EncoderState* state);

} // namespace platform
} // namespace sc
