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


//
// Created by lodsb on 31-Mar-25.
//

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <fstream>
#include <algorithm>

#include <json.hpp>

#include "sc_control_mapping.h"
#include "sc_settings.h"
#include "global.h"
#include "../control/mapping_registry.h"
#include "../util/log.h"

// JSON serialization for enums - must be in global namespace to match enum definitions
NLOHMANN_JSON_SERIALIZE_ENUM( EventType, {
    {EventType::BUTTON_HOLDING, "button_holding"},
    {EventType::BUTTON_HOLDING_SHIFTED, "button_holding_shifted"},
    {EventType::BUTTON_PRESSED, "button_pressed"},
    {EventType::BUTTON_PRESSED_SHIFTED, "button_pressed_shifted"},
    {EventType::BUTTON_RELEASED, "button_released"},
    {EventType::BUTTON_RELEASED_SHIFTED, "button_released_shifted"},
})

NLOHMANN_JSON_SERIALIZE_ENUM( MIDIStatusType, {
    {MIDIStatusType::MIDI_NOTE_ON, "midi_note_on"},
    {MIDIStatusType::MIDI_NOTE_OFF, "midi_note_off"},
    {MIDIStatusType::MIDI_CC, "midi_cc"},
    {MIDIStatusType::MIDI_PB, "midi_pb"},
})

NLOHMANN_JSON_SERIALIZE_ENUM( ActionType, {
   {ActionType::CUE, "cue"},
   {ActionType::SHIFTON, "shift_on"},
   {ActionType::SHIFTOFF, "shift_off"},
   {ActionType::STARTSTOP, "start_stop"},
   {ActionType::START, "start"},
   {ActionType::STOP, "stop"},
   {ActionType::PITCH, "pitch"},
   {ActionType::NOTE, "note"},
   {ActionType::GND, "gnd"},
   {ActionType::VOLUME, "volume"},
   {ActionType::NEXTFILE, "next_file"},
   {ActionType::PREVFILE, "prev_file"},
   {ActionType::RANDOMFILE, "random_file"},
   {ActionType::NEXTFOLDER, "next_folder"},
   {ActionType::PREVFOLDER, "prev_folder"},
   {ActionType::RECORD, "record"},
   {ActionType::LOOPERASE, "loop_erase"},
   {ActionType::LOOPRECALL, "loop_recall"},
   {ActionType::VOLUP, "volume_up"},
   {ActionType::VOLDOWN, "volume_down"},
   {ActionType::JOGPIT, "jog_pit"},
   {ActionType::DELETECUE, "delete_cue"},
   {ActionType::SC500, "sc500"},
   {ActionType::VOLUHOLD, "volume_up_hold"},
   {ActionType::VOLDHOLD, "volume_down_hold"},
   {ActionType::JOGPSTOP, "jog_pstop"},
   {ActionType::JOGREVERSE, "jog_reverse"},
   {ActionType::BEND, "bend"},
   {ActionType::NOTHING, "nothing"},
})

NLOHMANN_JSON_SERIALIZE_ENUM( audio_interface_type, {
   {AUDIO_TYPE_MAIN, "main"},
   {AUDIO_TYPE_USB, "usb"},
   {AUDIO_TYPE_CUSTOM, "custom"},
})

NLOHMANN_JSON_SERIALIZE_ENUM( output_channel_type, {
   {OUT_NONE, "none"},
   {OUT_AUDIO, "audio"},
   {OUT_CV_PLATTER_SPEED, "cv_platter_speed"},
   {OUT_CV_SAMPLE_POSITION, "cv_sample_position"},
   {OUT_CV_CROSSFADER, "cv_crossfader"},
   {OUT_CV_GATE_A, "cv_gate_a"},
   {OUT_CV_GATE_B, "cv_gate_b"},
   {OUT_CV_PLATTER_ANGLE, "cv_platter_angle"},
   {OUT_CV_PLATTER_ACCEL, "cv_platter_accel"},
   {OUT_CV_DIRECTION_PULSE, "cv_direction_pulse"},
})

namespace sc {
namespace config {

// Default importer path
constexpr const char* DEFAULT_IMPORTER_PATH = "/root/sc1000-import";

void add_mapping(sc::control::MappingRegistry& registry, IOType type, unsigned char deck_no, unsigned char *buf, unsigned char port, unsigned char pin, bool pullup, EventType edge_type, ActionType action, unsigned char parameter)
{
   Mapping new_map{};

   new_map.type      = type;
   new_map.pin       = pin;
   new_map.gpio_port = port;
   new_map.pullup    = pullup;

   // Runtime state (debounce, shifted_at_press) is managed separately in ButtonState

   if (buf != nullptr)
   {
      new_map.midi_command_bytes[0] = buf[0];
      new_map.midi_command_bytes[1] = buf[1];
      new_map.midi_command_bytes[2] = buf[2];
   }

   new_map.edge_type = edge_type;
   new_map.action_type = action;
   new_map.parameter = parameter;

   new_map.deck_no = deck_no;

   registry.add(new_map);
}

void settings_from_json(ScSettings* settings, const nlohmann::json& json)
{
   // Set defaults first - these are used if JSON keys are missing
   settings->period_size = json.value("period_size", 256u);
   settings->buffer_period_factor = json.value("buffer_period_factor", 4u);
   settings->sample_rate = json.value("sample_rate", 48000);
   settings->single_vca = static_cast<char>(json.value("single_vca", 0));
   settings->double_cut = static_cast<char>(json.value("double_cut", 0));
   settings->hamster = static_cast<char>(json.value("hamster", 0));
   settings->fader_close_point = json.value("fader_close_point", 2);
   settings->fader_open_point = json.value("fader_open_point", 10);
   settings->update_rate = json.value("update_rate", 2000);
   settings->platter_enabled = json.value("platter_enabled", true);
   settings->platter_speed = json.value("platter_speed", 2275);
   settings->debounce_time = json.value("debounce_time", 5);
   settings->hold_time = json.value("hold_time", 100);
   settings->slippiness = json.value("slippiness", 200);
   settings->brake_speed = json.value("brake_speed", 3000);
   settings->max_scratch_pitch = json.value("max_scratch_pitch", 10.0);
   settings->pitch_range = json.value("pitch_range", 50);
   settings->midi_init_delay = json.value("midi_init_delay", 5u);
   settings->audio_init_delay = json.value("audio_init_delay", 2u);
   settings->disable_volume_adc = json.value("disable_volume_adc", false);
   settings->disable_pic_buttons = json.value("disable_pic_buttons", false);
   settings->volume_amount = json.value("volume_amount", 0.03);
   settings->volume_amount_held = json.value("volume_amount_held", 0.001);
   settings->initial_volume = json.value("initial_volume", 0.125);
   settings->max_volume = json.value("max_volume", 1.0);
   settings->midi_remapped = 0;
   settings->io_remapped = 0;
   settings->jog_reverse = json.value("jog_reverse", false);
   settings->cut_beats = json.value("cut_beats", 0);
   settings->importer = DEFAULT_IMPORTER_PATH;

   // Loop recording settings
   settings->loop_max_seconds = json.value("loop_max_seconds", 60);

   // Crossfader ADC calibration
   settings->crossfader_adc_min = json.value("crossfader_adc_min", 0);
   settings->crossfader_adc_max = json.value("crossfader_adc_max", 1023);
}

void add_midi_mapping_from_json(sc::control::MappingRegistry& mappings, const nlohmann::json& json)
{
   const MIDIStatusType midi_status = json["type"].template get<MIDIStatusType>();
   const bool shifted = json["shifted"].template get<bool>();

   // Derive event type from MIDI status AND shifted flag
   // note_off -> BUTTON_RELEASED, note_on/cc/pb -> BUTTON_PRESSED
   EventType event;
   if (midi_status == MIDI_NOTE_OFF) {
      event = shifted ? EventType::BUTTON_RELEASED_SHIFTED : EventType::BUTTON_RELEASED;
   } else {
      event = shifted ? EventType::BUTTON_PRESSED_SHIFTED : EventType::BUTTON_PRESSED;
   }

   const unsigned char channel = json["channel"].template get<unsigned char>();
   const unsigned char parameter1 = json["parameter1"].template get<unsigned char>();
   const unsigned char parameter2 = json["parameter2"].template get<unsigned char>();
   const auto deck_string = json["deck"].template get<std::string>();
   const unsigned char deck_no = deck_string == "beats" ? 0 : 1;
   const ActionType action = json["action"].template get<ActionType>();

   unsigned char midi_command[3];

   const auto control_type_byte = static_cast<unsigned char>(midi_status);

   // Wildcard expansion for note_on and note_off (parameter1 == 255)
   if ((midi_status == MIDI_NOTE_ON || midi_status == MIDI_NOTE_OFF) && parameter1 == 255)
   {
      for (unsigned char note_number = 0; note_number < 128; note_number++)
      {
         midi_command[ 0 ] = static_cast<unsigned char>((control_type_byte << 4) | channel);
         midi_command[ 1 ] = note_number;
         midi_command[ 2 ] = 0;

         if (action == ActionType::NOTE)
         {
            add_mapping(mappings, IOType::MIDI, deck_no, midi_command, 0, 0, false, event, action, note_number);
         }
         else
         {
            add_mapping(mappings, IOType::MIDI, deck_no, midi_command, 0, 0, false, event, action, 0);
         }
      }
   }
   else
   {
      midi_command[ 0 ] = static_cast<unsigned char>((control_type_byte << 4) | channel);
      midi_command[ 1 ] = parameter1;
      midi_command[ 2 ] = 0;

      add_mapping(mappings, IOType::MIDI, deck_no, midi_command, 0, 0, false, event, action, parameter2);
   }
}

void add_gpio_mapping_from_json(sc::control::MappingRegistry& mappings, const nlohmann::json& json)
{
   const EventType event = json["event"].template get<EventType>();
   const unsigned char port = json["port"].template get<unsigned char>();
   const unsigned char pin = json["pin"].template get<unsigned char>();
   const bool pull_up = json["pull_up"].template get<bool>();
   const auto deck_string = json["deck"].template get<std::string>();
   const unsigned char deck_no = deck_string == "beats" ? 0 : 1;
   ActionType action = json["action"].template get<ActionType>();

   // Optional parameter field (used for cue button index in auto-cue combo detection)
   const unsigned char parameter = json.value("parameter", static_cast<unsigned char>(0));

   add_mapping(mappings, IOType::IO, deck_no, nullptr, port, pin, pull_up, event, action, parameter);
}

// Note: Legacy sc_settings_old_format() function removed - now using JSON config only

void load_json_config(ScSettings* settings, sc::control::MappingRegistry& mappings)
{
   std::ifstream f;

   // Try several locations for settings file:
   // 1. Current directory (for desktop development)
   // 2. Root path from settings (if already set)
   // 3. Default hardware paths
   const char* paths[] = {
      "./sc_settings.json",
      "../sc_settings.json",
      "/media/sda/sc_settings.json",
      "/var/sc_settings.json",
      nullptr
   };

   for (int i = 0; paths[i] != nullptr; i++)
   {
      f.open(paths[i], std::ios::in);
      if (!f.fail())
      {
         std::cerr << "Loaded settings from: " << paths[i] << std::endl;
         break;
      }
      f.clear();  // Clear fail state before trying next path
   }

   if (f.fail())
   {
      std::cerr << "Could not open any settings file, exiting" << std::endl;
      std::cerr << "Searched: ./sc_settings.json, ../sc_settings.json, /media/sda/sc_settings.json, /var/sc_settings.json" << std::endl;
      exit(-1);
   }

   try
   {
      auto json_main = nlohmann::json::parse(f);

      // Get settings section, use empty object if missing
      auto json_settings = json_main.value("sc1000", nlohmann::json::object());
      settings_from_json(settings, json_settings);

      // Load GPIO mappings if present
      if (json_main.contains("gpio_mapping") && json_main["gpio_mapping"].is_array())
      {
         for (const auto& m : json_main["gpio_mapping"])
         {
            try {
               add_gpio_mapping_from_json(mappings, m);
            } catch (const nlohmann::json::exception& e) {
               std::cerr << "Warning: Invalid GPIO Mapping entry: " << e.what() << std::endl;
            }
         }
      }

      // Load MIDI mappings if present
      if (json_main.contains("midi_mapping") && json_main["midi_mapping"].is_array())
      {
         for (const auto& m : json_main["midi_mapping"])
         {
            try {
               add_midi_mapping_from_json(mappings, m);
            } catch (const nlohmann::json::exception& e) {
               std::cerr << "Warning: Invalid MIDI Mapping entry: " << e.what() << std::endl;
            }
         }
      }

      // Load audio devices if present
      if (json_main.contains("audio_devices") && json_main["audio_devices"].is_array())
      {
         settings->audio_interfaces.clear();
         for (const auto& dev : json_main["audio_devices"])
         {
            try {
               AudioInterface iface{};

               // Human-readable name
               iface.name = dev.value("name", "Audio Device");

               // ALSA device identifier
               iface.device = dev.value("device", "hw:0");

               // Type
               iface.type = dev.value("type", AUDIO_TYPE_MAIN);

               // Channels
               iface.channels = dev.value("channels", 2);

               // Sample rate (inherit from main settings if not specified)
               iface.sample_rate = dev.value("sample_rate", settings->sample_rate);

               // Period size (inherit from main settings if not specified)
               iface.period_size = dev.value("period_size", static_cast<int>(settings->period_size));

               // Buffer factor (inherit from main settings if not specified)
               iface.buffer_period_factor = dev.value("buffer_period_factor",
                                                       static_cast<int>(settings->buffer_period_factor));

               // Capabilities
               iface.supports_cv = dev.value("supports_cv", false);

               // Input channel configuration (for recording)
               iface.input_channels = dev.value("input_channels", 0);
               iface.input_left = dev.value("input_left", 0);
               iface.input_right = dev.value("input_right", 1);

               // Initialize output map to none
               for (int i = 0; i < MAX_OUTPUT_CHANNELS; i++) {
                  iface.output_map[i] = OUT_NONE;
               }
               iface.num_mapped_outputs = 0;

               // Parse output_map if present: { "audio": 0, "cv_platter_speed": 2, ... }
               // Auto-detect required channels from Mapping
               int max_channel_needed = 0;

               if (dev.contains("output_map") && dev["output_map"].is_object())
               {
                  for (auto& [key, val] : dev["output_map"].items())
                  {
                     int hw_channel = val.get<int>();
                     if (hw_channel >= 0 && hw_channel < MAX_OUTPUT_CHANNELS)
                     {
                        // Parse the logical channel type from the key
                        output_channel_type logical = OUT_NONE;
                        if (key == "audio") logical = OUT_AUDIO;
                        else if (key == "cv_platter_speed") logical = OUT_CV_PLATTER_SPEED;
                        else if (key == "cv_sample_position") logical = OUT_CV_SAMPLE_POSITION;
                        else if (key == "cv_crossfader") logical = OUT_CV_CROSSFADER;
                        else if (key == "cv_gate_a") logical = OUT_CV_GATE_A;
                        else if (key == "cv_gate_b") logical = OUT_CV_GATE_B;
                        else if (key == "cv_platter_angle") logical = OUT_CV_PLATTER_ANGLE;
                        else if (key == "cv_platter_accel") logical = OUT_CV_PLATTER_ACCEL;
                        else if (key == "cv_direction_pulse") logical = OUT_CV_DIRECTION_PULSE;

                        if (logical != OUT_NONE)
                        {
                           iface.output_map[hw_channel] = logical;
                           iface.num_mapped_outputs++;

                           // Audio is stereo, takes 2 channels
                           if (logical == OUT_AUDIO)
                           {
                              max_channel_needed = std::max(max_channel_needed, hw_channel + 2);
                              iface.num_mapped_outputs++;
                           }
                           else
                           {
                              // CV sources are mono
                              max_channel_needed = std::max(max_channel_needed, hw_channel + 1);
                           }
                        }
                     }
                  }

                  // Auto-set channels from Mapping (override config if Mapping requires more)
                  if (max_channel_needed > iface.channels)
                  {
                     iface.channels = max_channel_needed;
                  }
               }
               else
               {
                  // No output_map - default stereo audio on channels 0,1
                  iface.output_map[0] = OUT_AUDIO;
                  iface.num_mapped_outputs = 2;
                  if (iface.channels < 2)
                  {
                     iface.channels = 2;
                  }
               }

               std::cout << "Audio config: " << iface.name
                         << " (" << iface.device << ")"
                         << " out_ch=" << iface.channels
                         << " in_ch=" << iface.input_channels
                         << " cv=" << iface.supports_cv
                         << " mapped=" << iface.num_mapped_outputs << std::endl;

               // Debug: print output map
               for (int ch = 0; ch < iface.channels && ch < MAX_OUTPUT_CHANNELS; ch++)
               {
                  if (iface.output_map[ch] != OUT_NONE)
                  {
                     const char* type_name = "unknown";
                     switch (iface.output_map[ch])
                     {
                        case OUT_AUDIO: type_name = "audio"; break;
                        case OUT_CV_PLATTER_SPEED: type_name = "cv_platter_speed"; break;
                        case OUT_CV_SAMPLE_POSITION: type_name = "cv_sample_position"; break;
                        case OUT_CV_CROSSFADER: type_name = "cv_crossfader"; break;
                        case OUT_CV_GATE_A: type_name = "cv_gate_a"; break;
                        case OUT_CV_GATE_B: type_name = "cv_gate_b"; break;
                        case OUT_CV_PLATTER_ANGLE: type_name = "cv_platter_angle"; break;
                        case OUT_CV_PLATTER_ACCEL: type_name = "cv_platter_accel"; break;
                        case OUT_CV_DIRECTION_PULSE: type_name = "cv_direction_pulse"; break;
                        default: break;
                     }
                     std::cout << "  ch" << ch << " -> " << type_name << std::endl;
                  }
               }

               settings->audio_interfaces.push_back(std::move(iface));

            } catch (const nlohmann::json::exception& e) {
               std::cerr << "Warning: Invalid audio device entry: " << e.what() << std::endl;
            }
         }
      }
      else
      {
         // No audio_devices section - create default main device
         sc_settings_init_default_audio(settings);
      }
   }
   catch (const nlohmann::json::parse_error& e)
   {
      std::cerr << "JSON parse error: " << e.what() << std::endl;
      std::cerr << "Using default settings" << std::endl;
      // Apply defaults via empty JSON object
      settings_from_json(settings, nlohmann::json::object());
   }
   catch (const nlohmann::json::exception& e)
   {
      std::cerr << "JSON error: " << e.what() << std::endl;
      std::cerr << "Using default settings" << std::endl;
      settings_from_json(settings, nlohmann::json::object());
   }
}

} // namespace config
} // namespace sc

// C++ API for loading configuration
void sc_settings_load_user_configuration(ScSettings* settings, sc::control::MappingRegistry& mappings)
{
   sc::config::load_json_config(settings, mappings);
}

void sc_settings_print_gpio_mappings(const sc::control::MappingRegistry& mappings)
{
   LOG_INFO("=== GPIO Mappings Loaded ===");

   static const char* action_names[] = {
      "CUE", "SHIFTON", "SHIFTOFF", "STARTSTOP", "START", "STOP",
      "PITCH", "NOTE", "GND", "VOLUME", "NEXTFILE", "PREVFILE",
      "RANDOMFILE", "NEXTFOLDER", "PREVFOLDER", "RECORD", "LOOPERASE",
      "LOOPRECALL", "VOLUP", "VOLDOWN", "JOGPIT", "DELETECUE", "SC500",
      "VOLUHOLD", "VOLDHOLD", "JOGPSTOP", "JOGREVERSE", "BEND", "NOTHING"
   };

   static const char* edge_names[] = {
      "RELEASED", "PRESSED", "HOLDING", "PRESSED_SHIFTED",
      "HOLDING_SHIFTED", "RELEASED_SHIFTED"
   };

   int gpio_count = 0;
   int midi_count = 0;

   for (const auto& m : mappings.all())
   {
      if (m.type == IOType::IO)
      {
         const char* action_str = (m.action_type < 29) ? action_names[m.action_type] : "UNKNOWN";
         const char* edge_str = (m.edge_type < 6) ? edge_names[m.edge_type] : "UNKNOWN";
         LOG_DEBUG("  GPIO port=%d pin=%2d deck=%d action=%-12s event=%-16s",
                   m.gpio_port, m.pin, m.deck_no, action_str, edge_str);
         gpio_count++;
      }
      else
      {
         midi_count++;
      }
   }

   LOG_INFO("=== Total: %d GPIO, %d MIDI mappings ===", gpio_count, midi_count);

   // Log pitch bend mappings specifically for debugging
   LOG_INFO("=== Pitch Bend Mappings ===");
   int pb_count = 0;
   for (const auto& m : mappings.all())
   {
      if (m.type == IOType::MIDI && ((m.midi_command_bytes[0] & 0xF0) == 0xE0))
      {
         const char* action_str = (m.action_type < 29) ? action_names[m.action_type] : "UNKNOWN";
         const char* edge_str = (m.edge_type < 6) ? edge_names[m.edge_type] : "UNKNOWN";
         LOG_INFO("  PB ch=%d deck=%d action=%-12s event=%-16s midi_cmd=[%02X]",
                  m.midi_command_bytes[0] & 0x0F,
                  m.deck_no, action_str, edge_str,
                  m.midi_command_bytes[0]);
         pb_count++;
      }
   }
   if (pb_count == 0) {
      LOG_INFO("  (no pitch bend mappings found)");
   }
}

AudioInterface* sc_settings_get_audio_interface( ScSettings* settings, audio_interface_type type )
{
   for (auto& iface : settings->audio_interfaces)
   {
      if (iface.type == type)
      {
         return &iface;
      }
   }
   return nullptr;
}

void sc_settings_init_default_audio( ScSettings* settings )
{
   settings->audio_interfaces.clear();

   AudioInterface iface{};
   iface.name = "Internal Codec";
   iface.device = "hw:0";
   iface.type = AUDIO_TYPE_MAIN;
   iface.channels = 2;
   iface.sample_rate = settings->sample_rate;
   iface.period_size = static_cast<int>(settings->period_size);
   iface.buffer_period_factor = static_cast<int>(settings->buffer_period_factor);
   iface.supports_cv = false;
   iface.input_channels = 0;  // Internal codec typically has no inputs
   iface.input_left = 0;
   iface.input_right = 1;

   // Default stereo Mapping (output_map already zeroed by default initialization)
   iface.output_map[0] = OUT_AUDIO;
   iface.num_mapped_outputs = 2;  // Audio is stereo

   settings->audio_interfaces.push_back(std::move(iface));
}

int sc_settings_get_output_channel( AudioInterface* iface, output_channel_type logical )
{
   if (iface == nullptr) return -1;

   for (int i = 0; i < MAX_OUTPUT_CHANNELS; i++)
   {
      if (iface->output_map[i] == logical)
      {
         return i;
      }
   }
   return -1;  // Not mapped
}

AudioInterface* sc_settings_find_cv_interface( ScSettings* settings )
{
   for (auto& iface : settings->audio_interfaces)
   {
      if (iface.supports_cv)
      {
         return &iface;
      }
   }
   return nullptr;
}