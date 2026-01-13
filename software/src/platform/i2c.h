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


// I2C communication primitives for SC1000 hardware
#pragma once

namespace sc {
namespace platform {

// Open an I2C device and set slave address
// Returns file descriptor on success, -1 on failure
int i2c_open(const char* path, unsigned char address);

// Read a single byte from an I2C register
void i2c_read_reg(int fd, unsigned char reg, unsigned char* result);

// Write a single byte to an I2C register
// Returns 1 on success, 0 on failure
int i2c_write_reg(int fd, unsigned char reg, unsigned char value);

} // namespace platform
} // namespace sc
