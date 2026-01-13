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


#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace sc { namespace control { class MappingRegistry; } }

// Maximum output channels per interface
#define MAX_OUTPUT_CHANNELS 16

// Audio interface types
enum audio_interface_type {
   AUDIO_TYPE_MAIN = 0,     // Main stereo output (simple, like sun4i-codec)
   AUDIO_TYPE_USB = 1,      // USB audio device (multi-channel, CV capable)
   AUDIO_TYPE_CUSTOM = 2    // Custom/other
};

// Logical output channel types
enum output_channel_type {
   OUT_NONE = 0,            // Unmapped/unused

   // Audio outputs (stereo pair starting at mapped channel)
   OUT_AUDIO,               // Main stereo mix (scratch + beat)

   // CV sources (mono, calculated only if mapped)
   OUT_CV_PLATTER_SPEED,    // Bipolar: platter rotation speed (-1 to +1)
   OUT_CV_SAMPLE_POSITION,  // Unipolar: relative position in sample (0 to 1)
   OUT_CV_CROSSFADER,       // Crossfader position
   OUT_CV_GATE_A,           // Gate: high when fader at A end (scratch open)
   OUT_CV_GATE_B,           // Gate: high when fader at B end (beat open)
   OUT_CV_PLATTER_ANGLE,    // Unipolar: absolute platter angle (0 to 1, saw LFO)
   OUT_CV_PLATTER_ACCEL,    // Bipolar: platter acceleration
   OUT_CV_DIRECTION_PULSE,  // Trigger: pulse on platter direction change
};

// Audio interface configuration
// Devices are listed in priority order - first available match is used
struct AudioInterface {
   std::string name;              // Human-readable name, e.g., "Bitwig Connect"
   std::string device;            // ALSA device name, e.g., "hw:0", "plughw:1"
   audio_interface_type type = AUDIO_TYPE_MAIN;
   int channels = 2;              // Number of hardware channels
   int sample_rate = 48000;       // Sample rate
   int period_size = 256;         // Period size
   int buffer_period_factor = 4;  // Buffer = period * factor
   bool supports_cv = false;      // Whether this device can output CV

   // Input channel configuration (for recording)
   int input_channels = 0;        // Number of capture channels (0 = no capture)
   int input_left = 0;            // Which capture channel is left
   int input_right = 1;           // Which capture channel is right

   // Output channel Mapping: output_map[hw_channel] = logical_type
   // e.g., output_map[4] = OUT_CV1 means hardware channel 4 outputs CV1
   output_channel_type output_map[MAX_OUTPUT_CHANNELS] = {};
   int num_mapped_outputs = 0;    // How many channels are mapped
};

struct ScSettings
{
   // output buffer size, probably 256
   unsigned int period_size;
   unsigned int buffer_period_factor;

   // sample rate, probably 48000
   int sample_rate;

   // fader options
   char single_vca;
   char double_cut;
   char hamster;

   // fader thresholds for hysteresis
   int fader_open_point; // value required to open the fader (when fader is closed)
   int fader_close_point; // value required to close the fader (when fader is open)

   // delay between iterations of the input loop
   int update_rate;

   // Whether platter input is enabled
   bool platter_enabled;

   // specifies the ratio of platter movement to sample movement
   // 4096 = 1 second for every platter rotation
   // Default 3072 = 1.33 seconds for every platter rotation
   int platter_speed;

   // How long to debounce external GPIO switches
   int debounce_time;

   // How long a button press counts as a hold
   int hold_time;

   // Virtual slipmat slippiness - how quickly the sample returns to normal speed after you let go of the jog wheel
   // Higher values are slippier
   int slippiness;

   // How long the the platter takes to stop after you hit a stop button, higher values are longer
   int brake_speed;

   // Maximum scratch pitch (speed multiplier) when platter is touched
   // Higher values = faster response to quick movements, but can cause overshoot
   // Default 10.0, range ~5-20
   double max_scratch_pitch;

   // Pitch range of MIDI commands
   int pitch_range;

   // How many seconds to wait before initializing MIDI
   unsigned int midi_init_delay;

   // How many seconds to wait before initializing Audio
   unsigned int audio_init_delay;

   // Whether to take input from the volume knobs (SC500 disables this)
   bool disable_volume_adc;

   // Whether to take input from the PIC buttons (SC500 disables this)
   bool disable_pic_buttons;

   // how much to adjust the volumes when the volume up/down buttons are pressed or held
   double volume_amount;
   double volume_amount_held;

   // Whether to reverse the jogwheel direction
   bool jog_reverse;

   // Fader cut mode: 0 = off, 1 = cuts beats, 2 = cuts scratch
   int cut_beats;

   double initial_volume;
   double max_volume;  // Maximum output volume (0.0-1.0), default 1.0. Useful for SC500 which is loud at full volume.

   bool midi_remapped;
   bool io_remapped;

   std::string importer;

   // Audio interfaces (listed in priority order)
   std::vector<AudioInterface> audio_interfaces;

   // Loop recording settings
   int loop_max_seconds;        // Maximum loop recording duration (default 60)

   // Crossfader ADC calibration (for CV gates)
   int crossfader_adc_min;      // ADC value at beat side extreme (default 0)
   int crossfader_adc_max;      // ADC value at scratch side extreme (default 1023)

   // Root directory for samples, settings, etc.
   // Default: /media/sda (hardware), can be overridden via --root CLI arg
   std::string root_path;
};

#include "sc_input.h"

// Settings loading and utility functions
void sc_settings_load_user_configuration(ScSettings* settings, sc::control::MappingRegistry& mappings);
void sc_settings_print_gpio_mappings(const sc::control::MappingRegistry& mappings);
AudioInterface* sc_settings_get_audio_interface(ScSettings* settings, audio_interface_type type);
void sc_settings_init_default_audio(ScSettings* settings);
int sc_settings_get_output_channel(AudioInterface* iface, output_channel_type logical);
AudioInterface* sc_settings_find_cv_interface(ScSettings* settings);

