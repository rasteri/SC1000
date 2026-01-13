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


// PIC input processor interface (ADCs, buttons, capsense)
#pragma once

#include <cstdint>

namespace sc {
namespace platform {

struct PicState {
    int i2c_fd = -1;
    bool present = false;
};

// Raw readings from PIC
struct PicReadings {
    uint16_t adc[4];       // 10-bit ADC values (faders, pots)
    bool buttons[4];       // Button states
    bool cap_touched;      // Capacitive touch sensor
};

// Initialize PIC input processor on I2C bus
bool pic_init(PicState* state);

// Read all inputs from PIC
PicReadings pic_read_all(PicState* state);

} // namespace platform
} // namespace sc
