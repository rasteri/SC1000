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


// Desktop platform mock for testing without hardware
// Provides keyboard-controlled inputs for testing SC1000 software
#pragma once

#include <cstdint>
#include <atomic>

namespace sc {
namespace desktop {

// Mock platform state controlled by keyboard
struct DesktopPlatformState {
    // Encoder (platter position)
    std::atomic<int32_t> encoder_angle{0};     // 0-4095

    // Fader (crossfader position)
    std::atomic<uint16_t> fader_position{2048}; // 0-4095, center at 2048

    // Capacitive touch
    std::atomic<bool> cap_touch{false};

    // Buttons (simulated GPIO)
    std::atomic<uint16_t> button_state{0};      // 16 buttons

    // Control flags
    std::atomic<bool> running{true};

    // Display update needed
    std::atomic<bool> display_dirty{true};
};

// Initialize terminal for raw input
void terminal_init();

// Restore terminal to normal mode
void terminal_cleanup();

// Process keyboard input and update platform state
// Returns false when quit is requested
bool process_keyboard(DesktopPlatformState* state);

// Draw current state to terminal
void draw_state(const DesktopPlatformState* state);

// Print controls help
void print_controls();

} // namespace desktop
} // namespace sc
