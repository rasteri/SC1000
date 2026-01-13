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


#pragma once

#include <cstdint>

namespace sc {
namespace control {

//
// InputState - Encapsulates global input modifier state
//
// Replaces the static ActionState members. Owned by Sc1000.
// Only accessed from input thread, so no synchronization needed.
//

class InputState {
public:
    // Shift key state
    bool is_shifted() const { return shifted_; }
    void set_shifted(bool v) { shifted_ = v; }

    // Pitch mode: 0=off, 1=beat deck, 2=scratch deck
    int pitch_mode() const { return pitch_mode_; }
    void set_pitch_mode(int mode) { pitch_mode_ = mode; }

    // Cue button combo tracking for auto-cue mode toggle
    // button_index: 0-3 for cue buttons 1-4
    void cue_button_pressed(int button_index) {
        if (button_index >= 0 && button_index <= 3) {
            held_cue_buttons_ |= (1 << button_index);
        }
    }

    // Returns which deck's auto-cue should be toggled:
    // 0 = no combo, 1 = scratch deck, 2 = beat deck
    int cue_button_released(int button_index) {
        if (button_index < 0 || button_index > 3) return 0;

        int result = 0;
        // Check for combos before clearing the bit
        // Buttons 0+1 (cue 1+2) → scratch deck (deck_no=1)
        // Buttons 2+3 (cue 3+4) → beat deck (deck_no=0)
        if ((held_cue_buttons_ & 0x03) == 0x03) {  // Buttons 0+1
            result = 1;  // scratch deck
        } else if ((held_cue_buttons_ & 0x0C) == 0x0C) {  // Buttons 2+3
            result = 2;  // beat deck
        }

        // Clear the released button
        held_cue_buttons_ &= static_cast<uint8_t>(~(1 << button_index));
        return result;
    }

private:
    bool shifted_ = false;
    int pitch_mode_ = 0;
    uint8_t held_cue_buttons_ = 0;  // Bitmask of currently held cue buttons (bits 0-3)
};

} // namespace control
} // namespace sc
