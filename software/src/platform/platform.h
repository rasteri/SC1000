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


// Platform abstraction layer for SC1000 hardware
// This is the fixed hardware interface - components here won't change
#pragma once

#include "i2c.h"
#include "gpio.h"
#include "encoder.h"
#include "pic.h"

namespace sc {
namespace platform {

// Combined hardware state for the SC1000 board
struct HardwareState {
    GpioState gpio;
    EncoderState encoder;
    PicState pic;
};

// Initialize all platform hardware
// Returns true if critical components initialized successfully
inline bool platform_init(HardwareState* hw)
{
    bool ok = true;

    // GPIO is optional but nice to have
    gpio_init_mcp23017(&hw->gpio);
    gpio_init_a13_mmap(&hw->gpio);

    // Encoder is critical for scratch control
    if (!encoder_init(&hw->encoder)) {
        ok = false;
    }

    // PIC is critical for ADCs and buttons
    if (!pic_init(&hw->pic)) {
        ok = false;
    }

    return ok;
}

} // namespace platform
} // namespace sc
