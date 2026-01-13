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


// GPIO hardware access for SC1000
// - MCP23017 I/O expander (external, via I2C)
// - A13 SoC GPIO (memory-mapped)
#pragma once

#include <cstdint>

namespace sc {
namespace platform {

// Hardware state for GPIO subsystem
struct GpioState {
    // MCP23017 (I2C GPIO expander)
    int mcp23017_fd = -1;
    bool mcp23017_present = false;

    // A13 SoC GPIO (memory-mapped)
    volatile void* gpio_base = nullptr;
    bool mmap_present = false;
};

// Initialize MCP23017 I/O expander
// Returns true if successful
bool gpio_init_mcp23017(GpioState* state);

// Initialize A13 SoC memory-mapped GPIO
// Returns true if successful
bool gpio_init_a13_mmap(GpioState* state);

// Configure a pin on MCP23017 as input with optional pullup
void gpio_mcp23017_set_pullup(GpioState* state, uint8_t pin, bool pullup);

// Configure pin direction on MCP23017 (true = input, false = output)
void gpio_mcp23017_set_direction(GpioState* state, uint8_t pin, bool input);

// Set output value on MCP23017
void gpio_mcp23017_write(GpioState* state, uint8_t pin, bool value);

// Read all 16 pins from MCP23017
// Returns 16-bit value with pin states (active-high, inverted from hardware)
uint16_t gpio_mcp23017_read_all(GpioState* state);

// Configure an A13 GPIO pin as input with pullup setting
// port: 1-6 (PB-PG), pin: 0-27
// pullup: 0=disable, 1=pullup, 2=pulldown
void gpio_a13_configure_input(GpioState* state, uint8_t port, uint8_t pin, uint8_t pullup);

// Read a single A13 GPIO pin
// Returns pin state (active-high, inverted from hardware)
bool gpio_a13_read_pin(GpioState* state, uint8_t port, uint8_t pin);

// Read all pins from an A13 GPIO port
uint32_t gpio_a13_read_port(GpioState* state, uint8_t port);

} // namespace platform
} // namespace sc
