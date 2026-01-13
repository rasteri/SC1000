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

//
// Crossfader - hardware crossfader input handling
//
// Reads raw ADC value from PIC and converts to normalized position.
// Position: 0.0 = beat side, 1.0 = scratch side
//

class Crossfader {
public:
    Crossfader() = default;

    // Set calibration range (ADC values at extremes)
    void set_calibration(int adc_min, int adc_max);

    // Update from raw ADC reading (called from input thread)
    // adc_value: raw 10-bit ADC (0-1023), inverted (low = scratch side)
    void update(int adc_value);

    // Get current position (0.0 = beat, 1.0 = scratch)
    double position() const { return position_; }

    // Get raw ADC value (for debugging/calibration)
    int raw_adc() const { return raw_adc_; }

private:
    double position_ = 0.5;  // Default to center
    int raw_adc_ = 512;

    // Calibration
    int adc_min_ = 0;
    int adc_max_ = 1023;
};
