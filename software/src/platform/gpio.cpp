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

#include "gpio.h"
#include "i2c.h"

#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#include "../util/log.h"

namespace sc {
namespace platform {

// MCP23017 register addresses
constexpr uint8_t MCP_IODIRA   = 0x00;  // I/O direction register A
constexpr uint8_t MCP_IODIRB   = 0x01;  // I/O direction register B
constexpr uint8_t MCP_GPPUA    = 0x0C;  // Pullup register A
constexpr uint8_t MCP_GPPUB    = 0x0D;  // Pullup register B
constexpr uint8_t MCP_GPIOA    = 0x12;  // GPIO register A
constexpr uint8_t MCP_GPIOB    = 0x13;  // GPIO register B

// A13 GPIO base address
constexpr uint32_t A13_GPIO_BASE = 0x01C20800;

bool gpio_init_mcp23017(GpioState* state)
{
    state->mcp23017_fd = i2c_open("/dev/i2c-1", 0x20);
    if (state->mcp23017_fd < 0) {
        LOG_WARN("Couldn't init external GPIO (MCP23017)");
        state->mcp23017_present = false;
        return false;
    }

    // Test write to verify communication
    if (!i2c_write_reg(state->mcp23017_fd, MCP_GPPUA, 0xFF)) {
        LOG_WARN("Couldn't communicate with MCP23017");
        state->mcp23017_present = false;
        return false;
    }

    state->mcp23017_present = true;

    // Default: all pins input with pullups enabled
    i2c_write_reg(state->mcp23017_fd, MCP_IODIRA, 0xFF);
    i2c_write_reg(state->mcp23017_fd, MCP_IODIRB, 0xFF);
    i2c_write_reg(state->mcp23017_fd, MCP_GPPUA, 0xFF);
    i2c_write_reg(state->mcp23017_fd, MCP_GPPUB, 0xFF);

    return true;
}

bool gpio_init_a13_mmap(GpioState* state)
{
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) {
        LOG_WARN("Unable to open /dev/mem");
        state->mmap_present = false;
        return false;
    }

    void* mapped = mmap(nullptr, 65536, PROT_READ | PROT_WRITE, MAP_SHARED,
                        fd, A13_GPIO_BASE & 0xFFFF0000);
    close(fd);

    if (mapped == MAP_FAILED) {
        LOG_WARN("Unable to mmap GPIO");
        state->mmap_present = false;
        return false;
    }

    // Offset to actual GPIO registers
    state->gpio_base = static_cast<char*>(mapped) + (A13_GPIO_BASE & 0xFFFF);
    state->mmap_present = true;

    return true;
}

void gpio_mcp23017_set_pullup(GpioState* state, uint8_t pin, bool pullup)
{
    if (!state->mcp23017_present || pin >= 16) return;

    uint8_t reg = (pin < 8) ? MCP_GPPUA : MCP_GPPUB;
    uint8_t bit = pin % 8;

    unsigned char current;
    i2c_read_reg(state->mcp23017_fd, reg, &current);

    if (pullup) {
        current |= (1 << bit);
    } else {
        current &= ~(1 << bit);
    }

    i2c_write_reg(state->mcp23017_fd, reg, current);
}

void gpio_mcp23017_set_direction(GpioState* state, uint8_t pin, bool input)
{
    if (!state->mcp23017_present || pin >= 16) return;

    uint8_t reg = (pin < 8) ? MCP_IODIRA : MCP_IODIRB;
    uint8_t bit = pin % 8;

    unsigned char current;
    i2c_read_reg(state->mcp23017_fd, reg, &current);

    if (input) {
        current |= (1 << bit);
    } else {
        current &= ~(1 << bit);
    }

    i2c_write_reg(state->mcp23017_fd, reg, current);
}

void gpio_mcp23017_write(GpioState* state, uint8_t pin, bool value)
{
    if (!state->mcp23017_present || pin >= 16) return;

    uint8_t reg = (pin < 8) ? MCP_GPIOA : MCP_GPIOB;
    uint8_t bit = pin % 8;

    unsigned char current;
    i2c_read_reg(state->mcp23017_fd, reg, &current);

    if (value) {
        current |= (1 << bit);
    } else {
        current &= ~(1 << bit);
    }

    i2c_write_reg(state->mcp23017_fd, reg, current);
}

uint16_t gpio_mcp23017_read_all(GpioState* state)
{
    if (!state->mcp23017_present) return 0;

    unsigned char bank_a, bank_b;
    i2c_read_reg(state->mcp23017_fd, MCP_GPIOA, &bank_a);
    i2c_read_reg(state->mcp23017_fd, MCP_GPIOB, &bank_b);

    uint16_t result = (static_cast<uint16_t>(bank_b) << 8) | bank_a;

    // Invert: hardware is active-low, return active-high
    return result ^ 0xFFFF;
}

void gpio_a13_configure_input(GpioState* state, uint8_t port, uint8_t pin, uint8_t pullup)
{
    if (!state->mmap_present || port < 1 || port > 6 || pin > 27) return;

    auto* base = static_cast<volatile char*>(const_cast<void*>(state->gpio_base));

    // Which config register (0-3) and bit position
    uint32_t config_reg_idx = pin >> 3;
    uint32_t config_shift = (pin % 8) * 4;

    // Which pull register (0-1) and bit position
    uint32_t pull_reg_idx = pin >> 4;
    uint32_t pull_shift = (pin % 16) * 2;

    // Port offset: each port has 0x24 bytes of registers
    uint32_t port_offset = port * 0x24;

    auto* config_reg = reinterpret_cast<volatile uint32_t*>(base + port_offset + config_reg_idx * 0x04);
    auto* pull_reg = reinterpret_cast<volatile uint32_t*>(base + port_offset + 0x1C + pull_reg_idx * 0x04);

    // Set as input (0b0000)
    uint32_t config_mask = ~(0xFU << config_shift);
    *config_reg = (*config_reg & config_mask);

    // Set pullup mode
    uint32_t pull_mask = ~(0x3U << pull_shift);
    *pull_reg = (*pull_reg & pull_mask) | (pullup << pull_shift);
}

bool gpio_a13_read_pin(GpioState* state, uint8_t port, uint8_t pin)
{
    if (!state->mmap_present || port < 1 || port > 6 || pin > 27) return false;

    auto* base = static_cast<volatile char*>(const_cast<void*>(state->gpio_base));
    auto* data_reg = reinterpret_cast<volatile uint32_t*>(base + port * 0x24 + 0x10);

    uint32_t data = *data_reg ^ 0xFFFFFFFF;  // Invert: active-low to active-high
    return (data >> pin) & 0x01;
}

uint32_t gpio_a13_read_port(GpioState* state, uint8_t port)
{
    if (!state->mmap_present || port < 1 || port > 6) return 0;

    auto* base = static_cast<volatile char*>(const_cast<void*>(state->gpio_base));
    auto* data_reg = reinterpret_cast<volatile uint32_t*>(base + port * 0x24 + 0x10);

    return *data_reg ^ 0xFFFFFFFF;  // Invert: active-low to active-high
}

} // namespace platform
} // namespace sc
