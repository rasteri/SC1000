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

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#include "../platform/alsa.h"
#include "../util/debug.h"
#include "../util/log.h"

#include "../player/deck.h"
#include "../player/track.h"

#include "../engine/audio_engine.h"
#include "sc1000.h"
#include "sc_settings.h"

void Sc1000::setup(struct Rt* rt, const char* root_path)
{
    LOG_INFO("SC1000 engine init (root: %s)", root_path);

    settings = std::make_unique<ScSettings>();
    mappings.clear();

    // Store root path in settings for use by other components
    settings->root_path = root_path;

    sc_settings_load_user_configuration(settings.get(), mappings);

    // Verify root_path wasn't corrupted by settings loading
    LOG_DEBUG("After settings load, root_path = '%s'", settings->root_path.c_str());

    // Print loaded mappings for debugging
    sc_settings_print_gpio_mappings(mappings);

    // Create two decks, both pointed at the same audio device
    scratch_deck.init(settings.get());
    beat_deck.init(settings.get());

    // Set deck numbers for loop navigation
    beat_deck.deck_no = 0;
    scratch_deck.deck_no = 1;

    // Tell deck0 to just play without considering inputs
    beat_deck.player.input.just_play = true;

    // Initialize audio hardware (creates AudioHardware instance)
    audio = alsa_create(this, settings.get());
    rt->set_engine(this);

    alsa_clear_config_cache();
}

void Sc1000::load_sample_folders()
{
    const std::string& root = settings->root_path;
    std::string samples_path = root + "/samples";
    std::string beats_path = root + "/beats";

    LOG_DEBUG("load_sample_folders called, root_path = '%s'", root.c_str());
    LOG_DEBUG("samples_path = '%s', beats_path = '%s'", samples_path.c_str(), beats_path.c_str());

    // Check for samples folder (only do USB mount dance for default /media/sda)
    if (root == "/media/sda" && access(samples_path.c_str(), F_OK) == -1) {
        // Not there, so presumably the boot script didn't manage to mount the drive
        // Maybe it hasn't initialized yet, or at least wasn't at boot time
        // We have to do it ourselves

        // Timeout after 12 sec, in which case emergency samples will be loaded
        for (int uscnt = 0; uscnt < 12; uscnt++) {
            LOG_INFO("Waiting for USB stick...");
            // Wait for /dev/sda1 to show up and then mount it
            if (access("/dev/sda1", F_OK) != -1) {
                LOG_INFO("Found USB stick, mounting!");
                (void)system("/bin/mount /dev/sda1 /media/sda");
                break;
            } else {
                // If not here yet, wait a second then check again
                sleep(1);
            }
        }
    }

    LOG_INFO("Loading beats from: %s", beats_path.c_str());
    LOG_INFO("Loading samples from: %s", samples_path.c_str());

    beat_deck.load_folder(beats_path.c_str());
    scratch_deck.load_folder(samples_path.c_str());

    if (!scratch_deck.nav_state.files_present) {
        // Load the default sentence if no sample files found on usb stick
        scratch_deck.player.set_track(
                         track_acquire_by_import(scratch_deck.importer.c_str(), "/var/scratchsentence.mp3"));
        LOG_DEBUG("Set default track ok");
        scratch_deck.cues.load_from_file(scratch_deck.player.track->path);
        LOG_DEBUG("Set cues ok");
        // Set the time back a bit so the sample doesn't start too soon
        scratch_deck.player.input.seek_to = -4.0;
        scratch_deck.player.input.target_position = -4.0;
    }
}

void Sc1000::clear()
{
    beat_deck.clear();
    scratch_deck.clear();

    // Audio hardware cleaned up automatically via unique_ptr
    audio.reset();

    // Settings cleaned up automatically via unique_ptr
    settings.reset();
}

void Sc1000::audio_start()
{
    if (audio) {
        audio->start();
    }
}

void Sc1000::audio_stop()
{
    if (audio) {
        audio->stop();
    }
}

ssize_t Sc1000::audio_pollfds(struct pollfd* pe, size_t z)
{
    if (audio) {
        return audio->pollfds(pe, z);
    }
    return 0;
}

void Sc1000::audio_handle()
{
    if (fault || !audio) {
        return;
    }

    if (audio->handle() != 0) {
        fault = true;
        LOG_ERROR("Error handling audio device; disabling it");
    }
}

// Helper to handle recording for a single deck
static void handle_single_deck_recording(Sc1000* engine, Deck* dk, int deck_no)
{
    Player* pl = &dk->player;

    if (!engine->audio) return;

    // Handle start recording request
    if (pl->input.record_start) {
        pl->input.record_start = false;  // Clear one-shot request

        if (!engine->audio->is_recording(deck_no)) {
            double current_pos = engine->audio->get_position(deck_no);
            if (engine->audio->start_recording(deck_no, current_pos)) {
                pl->input.beep_request = sc::BeepType::RecordingStart;
                LOG_DEBUG("Recording started on deck %d at position %.2f", deck_no, current_pos);
            } else {
                pl->input.beep_request = sc::BeepType::RecordingError;
                LOG_DEBUG("Failed to start recording on deck %d", deck_no);
            }
        }
    }

    // Handle stop recording request
    if (pl->input.record_stop) {
        pl->input.record_stop = false;  // Clear one-shot request

        if (engine->audio->is_recording(deck_no)) {
            // Check if this was a first recording (will define loop) or punch-in
            bool was_first_recording = !engine->audio->has_loop(deck_no);

            // Stop recording
            engine->audio->stop_recording(deck_no);

            // Navigate to loop position (position 0 in track list)
            dk->nav_state.file_idx = -1;

            // Switch player to use loop track (RT-safe)
            pl->input.source = sc::PlaybackSource::Loop;
            LOG_DEBUG("Recording stopped on deck %d, set source=Loop, current_file_idx=-1", deck_no);

            if (was_first_recording) {
                pl->input.seek_to = 0.0;
                pl->input.target_position = 0.0;
                pl->input.position_offset = 0.0;
            }

            pl->input.beep_request = sc::BeepType::RecordingStop;
            LOG_DEBUG("Recording stopped on deck %d, navigated to loop (position 0)", deck_no);
        }
    }
}

void Sc1000::handle_deck_recording()
{
    // Handle loop buffer recording for both decks (memory-based, for immediate scratching)
    handle_single_deck_recording(this, &beat_deck, 0);     // Beat deck = 0
    handle_single_deck_recording(this, &scratch_deck, 1);  // Scratch deck = 1
}
