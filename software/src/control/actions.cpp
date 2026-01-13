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


// Action dispatch for SC1000 control events

#include "actions.h"
#include "input_state.h"

#include <cmath>
#include <cstdio>

#include "../player/cues.h"
#include "../player/deck.h"
#include "../player/playlist.h"
#include "../player/track.h"
#include "../core/sc1000.h"
#include "../core/sc_settings.h"
#include "../platform/alsa.h"
#include "../util/log.h"

namespace sc {
namespace control {

void perform_action_for_deck(Deck* deck, const Mapping* map,
                             const unsigned char midi_buffer[3],
                             Sc1000* engine, ScSettings* settings,
                             InputState& input_state)
{
    switch (map->action_type) {
    case CUE: {
        unsigned int cuenum = (map->type == MIDI)
            ? map->midi_command_bytes[1]
            : (map->gpio_port * 32) + map->pin + 128;

        // Track cue button state for auto-cue combo detection
        // Button index derived from cuenum % 4 (for MIDI notes 0-3, 4-7, etc.)
        // Or use parameter field if explicitly set (1-4 maps to index 0-3)
        int button_idx = (map->parameter >= 1 && map->parameter <= 4)
            ? (map->parameter - 1)
            : static_cast<int>(cuenum % 4);

        // Handle button press/release for combo detection
        if (map->edge_type == BUTTON_PRESSED || map->edge_type == BUTTON_PRESSED_SHIFTED) {
            input_state.cue_button_pressed(button_idx);
            deck->cue(cuenum, engine);
        } else if (map->edge_type == BUTTON_RELEASED || map->edge_type == BUTTON_RELEASED_SHIFTED) {
            // Check for combo - returns 0=none, 1=scratch, 2=beat
            int combo_deck = input_state.cue_button_released(button_idx);
            if (combo_deck == 1) {
                engine->scratch_deck.cycle_auto_cue_mode();
                // Skip normal cue action when combo detected
            } else if (combo_deck == 2) {
                engine->beat_deck.cycle_auto_cue_mode();
                // Skip normal cue action when combo detected
            } else {
                // No combo - normal cue action on release (optional)
                // deck->cue(cuenum, engine);  // Uncomment if cue should also fire on release
            }
        } else {
            // Other edge types - just fire the cue
            deck->cue(cuenum, engine);
        }
        break;
    }
    case DELETECUE: {
        unsigned int cuenum = (map->type == MIDI)
            ? map->midi_command_bytes[1]
            : (map->gpio_port * 32) + map->pin + 128;
        deck->unset_cue(cuenum);
        break;
    }
    case NOTE: {
        // Check for note-off: status 0x80 or note-on with velocity 0
        bool is_note_off = (midi_buffer[0] & 0xF0) == 0x80 ||
                           ((midi_buffer[0] & 0xF0) == 0x90 && midi_buffer[2] == 0);
        if (is_note_off) {
            deck->player.input.pitch_note = 1.0;
            LOG_DEBUG("NOTE action: note-off, pitch reset to 1.0");
        } else {
            // Equal temperament: 2^(1/12) per semitone, 0x3C = middle C
            double new_pitch = pow(pow(2.0, 1.0 / 12.0), midi_buffer[1] - 0x3C);
            deck->player.input.pitch_note = new_pitch;
            LOG_INFO("NOTE action: note=%d -> pitch=%.3f", midi_buffer[1], new_pitch);
        }
        break;
    }
    case STARTSTOP:
        deck->player.input.stopped = !deck->player.input.stopped;
        break;

    case SHIFTON:
        LOG_DEBUG("SHIFTON action fired, shifted: %d -> true", input_state.is_shifted());
        input_state.set_shifted(true);
        break;

    case SHIFTOFF:
        LOG_DEBUG("SHIFTOFF action fired, shifted: %d -> false", input_state.is_shifted());
        input_state.set_shifted(false);
        break;

    case NEXTFILE:
        deck->next_file(engine, settings);
        break;

    case PREVFILE:
        deck->prev_file(engine, settings);
        break;

    case RANDOMFILE:
        deck->random_file(engine, settings);
        break;

    case NEXTFOLDER:
        deck->next_folder(engine, settings);
        break;

    case PREVFOLDER:
        deck->prev_folder(engine, settings);
        break;

    case VOLUME:
        deck->player.input.volume_knob = static_cast<double>(midi_buffer[2]) / 128.0;
        break;

    case PITCH:
        if (map->type == MIDI) {
            double pitch = 1.0;
            int semitone_range = map->parameter;

            // Pitch bend message: use 14-bit accuracy
            if ((midi_buffer[0] & 0xF0) == 0xE0) {
                unsigned int pval = (static_cast<unsigned int>(midi_buffer[2]) << 7) |
                                    static_cast<unsigned int>(midi_buffer[1]);
                double normalized = (static_cast<double>(pval) - 8192.0) / 8192.0;

                if (semitone_range > 0) {
                    double semitones = normalized * static_cast<double>(semitone_range);
                    pitch = std::pow(2.0, semitones / 12.0);
                    LOG_DEBUG("PITCH action: 14-bit pval=%u norm=%.3f semi=%.1f pitch=%.4f deck=%d",
                             pval, normalized, semitones, pitch, map->deck_no);
                } else {
                    pitch = (normalized * (static_cast<double>(settings->pitch_range) / 100.0)) + 1.0;
                    LOG_DEBUG("PITCH action: 14-bit pval=%u pitch=%.4f range=%d%% deck=%d",
                             pval, pitch, settings->pitch_range, map->deck_no);
                }
            } else {
                // 7-bit CC
                double normalized = (static_cast<double>(midi_buffer[2]) - 64.0) / 64.0;

                if (semitone_range > 0) {
                    double semitones = normalized * static_cast<double>(semitone_range);
                    pitch = std::pow(2.0, semitones / 12.0);
                    LOG_DEBUG("PITCH action: 7-bit val=%d semi=%.1f pitch=%.4f deck=%d",
                             midi_buffer[2], semitones, pitch, map->deck_no);
                } else {
                    pitch = (normalized * (static_cast<double>(settings->pitch_range) / 100.0)) + 1.0;
                    LOG_DEBUG("PITCH action: 7-bit val=%d pitch=%.4f range=%d%% deck=%d",
                             midi_buffer[2], pitch, settings->pitch_range, map->deck_no);
                }
            }
            deck->player.input.pitch_fader = pitch;
        }
        break;

    case JOGPIT:
        input_state.set_pitch_mode(map->deck_no + 1);
        LOG_DEBUG("Set Pitch Mode %d", input_state.pitch_mode());
        break;

    case JOGPSTOP:
        input_state.set_pitch_mode(0);
        break;

    case SC500:
        LOG_DEBUG("SC500 detected");
        break;

    case VOLUP:
        deck->player.input.volume_knob += settings->volume_amount;
        if (deck->player.input.volume_knob > 1.0)
            deck->player.input.volume_knob = 1.0;
        break;

    case VOLDOWN:
        deck->player.input.volume_knob -= settings->volume_amount;
        if (deck->player.input.volume_knob < 0.0)
            deck->player.input.volume_knob = 0.0;
        break;

    case VOLUHOLD:
        deck->player.input.volume_knob += settings->volume_amount_held;
        if (deck->player.input.volume_knob > 1.0)
            deck->player.input.volume_knob = 1.0;
        break;

    case VOLDHOLD:
        deck->player.input.volume_knob -= settings->volume_amount_held;
        if (deck->player.input.volume_knob < 0.0)
            deck->player.input.volume_knob = 0.0;
        break;

    case JOGREVERSE:
        LOG_DEBUG("Reversed Jog Wheel: %d", settings->jog_reverse);
        settings->jog_reverse = !settings->jog_reverse;
        LOG_DEBUG(" -> %d", settings->jog_reverse);
        break;

    case BEND:
        // Temporary pitch bend on top of other pitch values
        deck->player.input.pitch_bend = pow(pow(2.0, 1.0 / 12.0), map->parameter - 0x3C);
        break;

    default:
        break;
    }
}

void dispatch_event(const Mapping* map, const unsigned char midi_buffer[3],
                    Sc1000* engine, ScSettings* settings,
                    InputState& input_state)
{
    if (map == nullptr) return;

    // Determine target deck from Mapping (0=beat, 1=scratch)
    Deck* target = (map->deck_no == 0)
        ? &engine->beat_deck
        : &engine->scratch_deck;

    switch (map->action_type) {
    case RECORD:
        target->record(engine);
        break;

    case LOOPERASE:
        // Long-hold RECORD erases the loop and navigates to first file
        LOG_DEBUG("LOOPERASE triggered on deck %d, was source=%d, was current_file_idx=%d",
                  map->deck_no, static_cast<int>(target->player.input.source), target->nav_state.file_idx);
        if (engine->audio) engine->audio->reset_loop(map->deck_no);
        target->player.input.source = sc::PlaybackSource::File;

        // Navigate to first file (position 1, index 0)
        target->nav_state.file_idx = 0;
        LOG_DEBUG("LOOPERASE set source=File, current_file_idx=0");
        if (target->nav_state.files_present) {
            ScFile* file = target->playlist->get_file(target->nav_state.folder_idx, 0);
            if (file != nullptr) {
                target->player.set_track(track_acquire_by_import(target->importer.c_str(), file->full_path.c_str()));
                target->player.input.seek_to = 0.0;
                target->player.input.position_offset = 0.0;
                target->cues.load_from_file(target->player.track->path);
            }
        }

        target->player.input.beep_request = sc::BeepType::RecordingError;
        LOG_DEBUG("Loop erased on deck %d, navigated to file 0", map->deck_no);
        break;

    case LOOPRECALL:
        LOG_DEBUG("Loop recall triggered on deck %d", map->deck_no);
        if (target->recall_loop(settings)) {
            target->player.input.beep_request = sc::BeepType::RecordingStart;
        } else {
            target->player.input.beep_request = sc::BeepType::RecordingError;
        }
        break;

    default:
        perform_action_for_deck(target, map, midi_buffer, engine, settings, input_state);
        break;
    }
}

} // namespace control
} // namespace sc
