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


#include "crossfader.h"

void Crossfader::set_calibration(int adc_min, int adc_max)
{
    adc_min_ = adc_min;
    adc_max_ = adc_max;
}

void Crossfader::update(int adc_value)
{
    raw_adc_ = adc_value;

    int adc_range = adc_max_ - adc_min_;
    if (adc_range > 0) {
        // Clamp to calibration range
        int clamped = adc_value;
        if (clamped < adc_min_) clamped = adc_min_;
        if (clamped > adc_max_) clamped = adc_max_;

        // Normalize and invert (ADC is inverted: low = scratch side)
        double normalized = static_cast<double>(clamped - adc_min_) / adc_range;
        position_ = 1.0 - normalized;
    } else {
        // Fallback if bad calibration
        position_ = 0.5;
    }
}
