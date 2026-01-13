/*
 * Copyright (C) 2018 Mark Hills <mark@xwax.org>
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

#include <cassert>
#include <cstdlib>

#include "../core/global.h"
#include "../core/sc1000.h"
#include "../platform/alsa.h"

#include "../util/log.h"
#include "../util/status.h"
#include "../thread/rig.h"

#include "cues.h"
#include "deck.h"
#include "playlist.h"
#include "track.h"

//
// LoopState member - defined here where track is complete
//

bool LoopState::has_loop() const
{
	return track != nullptr && track->length > 0;
}

//
// Helper function (internal)
//

static void load_track_internal(struct Deck* d, Track* track, struct ScSettings* settings)
{
	struct Player* pl = &d->player;
	d->cues.save_to_file(pl->track->path);
	pl->set_track(track);

	// Use new input fields for position and state
	pl->input.seek_to = 0.0;
	pl->input.target_position = 0.0;
	pl->input.position_offset = 0.0;
	pl->input.source = sc::PlaybackSource::File;  // Switch back to file track (loop is preserved for recall)
	pl->input.stopped = false;   // Reset stopped state so scratching works immediately

	d->cues.load_from_file(pl->track->path);

	// Reset pitch to neutral
	pl->input.pitch_fader = 1.0;
	pl->input.pitch_bend = 1.0;
	pl->input.pitch_note = 1.0;

	// Reset cap_touch to force re-detection and proper angle_offset recalculation
	// (Part of encoder glitch protection chain - see audio_engine.cpp:173)
	pl->input.touched = false;

	// Reset auto-cue mode on track change (no persistence)
	d->reset_auto_cue_mode();

	if (!pl->input.just_play)
	{
		// Reset encoder offset (we're seeking to position 0)
		d->encoder_state.offset = 0 - d->encoder_state.angle;
	}
}

//
// C++ Member function implementations
//

// Destructor must be defined where Playlist is complete (for unique_ptr deletion)
Deck::~Deck() = default;

int Deck::init(struct ScSettings* settings)
{
	punch = std::nullopt;
	assert(!settings->importer.empty());
	importer = settings->importer;
	shifted = false;

	player.init(TARGET_SAMPLE_RATE, track_acquire_empty(), settings);
	cues.reset();

	// playlist is default-initialized to nullptr via unique_ptr
	nav_state.folder_idx = 0;
	nav_state.file_idx = 0;
	nav_state.files_present = false;
	deck_no = -1;  // Will be set by Sc1000 init

	encoder_state.offset = 0;
	encoder_state.angle = 0xffff;
	encoder_state.angle_raw = 0xffff;

	loop_state.track = nullptr;

	return 0;
}

void Deck::clear()
{
	player.clear();
	playlist.reset();

	if (loop_state.track)
	{
		track_release(loop_state.track);
		loop_state.track = nullptr;
	}
}

bool Deck::is_locked(struct Sc1000* /* engine */) const
{
	// Protection feature removed - decks are never locked
	return false;
}

void Deck::recue(struct Sc1000* engine)
{
	if (is_locked(engine))
	{
		status_printf(STATUS_WARN, "Stop deck to recue");
		return;
	}

	// Set offset to current position (elapsed = 0)
	double current_pos = engine && engine->audio ? engine->audio->get_position(deck_no) : 0.0;
	player.input.position_offset = current_pos;
}

void Deck::clone(const Deck& from, struct Sc1000* engine)
{
	// Copy input state and preserve elapsed time
	double from_elapsed = 0.0;
	double to_current = 0.0;
	if (engine && engine->audio) {
		from_elapsed = engine->audio->get_deck_state(from.deck_no).elapsed();
		to_current = engine->audio->get_position(deck_no);
	}
	player.input.position_offset = to_current - from_elapsed;
}

void Deck::unset_cue(unsigned int label)
{
	cues.unset(label);
}

void Deck::cue(unsigned int label, struct Sc1000* engine)
{
	// Auto-cue mode: calculate position from track divisions
	if (auto_cue_mode != AutoCueMode::Off && player.track != nullptr) {
		int divisions = auto_cue_divisions();
		if (divisions > 0) {
			// Map label to division index (wrap around)
			int slot = static_cast<int>(label) % divisions;

			// Calculate position in samples, then convert to seconds
			double track_length_samples = static_cast<double>(player.track->length);
			double slot_position_samples = (track_length_samples / divisions) * slot;
			double slot_position_seconds = slot_position_samples / player.track->rate;

			// Seek to calculated position
			player.input.seek_to = slot_position_seconds;
			player.input.position_offset = 0.0;  // Reset offset since we're seeking absolutely

			// For scratch deck: sync encoder and target_position for artifact-free jumps
			if (!player.input.just_play) {
				player.input.target_position = slot_position_seconds;
				// Sync encoder offset: new_pos = (angle + offset) / platter_speed
				// So offset = new_pos * platter_speed - angle
				int platter_speed = engine->settings->platter_speed;
				encoder_state.offset = static_cast<int32_t>(slot_position_seconds * platter_speed) - encoder_state.angle;
			}

			LOG_DEBUG("Auto-cue: slot=%d/%d pos=%.2fs", slot, divisions, slot_position_seconds);
			return;
		}
	}

	// Normal cue behavior
	auto p = cues.get(label);
	if (!p.has_value()) {
		// Set cue at current elapsed time
		double elapsed = engine && engine->audio ? engine->audio->get_deck_state(deck_no).elapsed() : 0.0;
		cues.set(label, elapsed);
		if (player.track != nullptr) {
			cues.save_to_file(player.track->path);
		}
	}
	else {
		// Seek to cue point: set offset so elapsed = p
		double current_pos = engine && engine->audio ? engine->audio->get_position(deck_no) : 0.0;
		player.input.position_offset = current_pos - p.value();

		// For scratch deck: sync encoder and target_position for artifact-free jumps
		if (!player.input.just_play) {
			player.input.target_position = p.value();
			int platter_speed = engine->settings->platter_speed;
			encoder_state.offset = static_cast<int32_t>(p.value() * platter_speed) - encoder_state.angle;
		}
	}
}

void Deck::punch_in(unsigned int label, struct Sc1000* engine)
{
	double elapsed = engine && engine->audio ? engine->audio->get_deck_state(deck_no).elapsed() : 0.0;
	auto p = cues.get(label);
	if (!p.has_value())
	{
		cues.set(label, elapsed);
		return;
	}

	double e = elapsed;
	if (punch.has_value())
		e -= punch.value();

	// Seek to cue point
	double current_pos = engine && engine->audio ? engine->audio->get_position(deck_no) : 0.0;
	player.input.position_offset = current_pos - p.value();
	punch = p.value() - e;
}

void Deck::punch_out(struct Sc1000* engine)
{
	if (!punch.has_value())
		return;

	double elapsed = engine && engine->audio ? engine->audio->get_deck_state(deck_no).elapsed() : 0.0;
	double target = elapsed - punch.value();
	double current_pos = engine && engine->audio ? engine->audio->get_position(deck_no) : 0.0;
	player.input.position_offset = current_pos - target;
	punch = std::nullopt;
}

void Deck::load_folder(const char* folder_name)
{
	playlist = std::make_unique<Playlist>();

	if (playlist->load(folder_name) && playlist->total_files() > 0)
	{
		LOG_INFO("Folder '%s' indexed with %zu files", folder_name, playlist->total_files());
		nav_state.files_present = true;
		nav_state.folder_idx = 0;
		nav_state.file_idx = 0;

		LOG_DEBUG("deck_load_folder");

		ScFile* file = playlist->get_file(0, 0);
		player.set_track(track_acquire_by_import(importer.c_str(), file->full_path.c_str()));
		LOG_DEBUG("deck_load_folder set track ok");
		cues.load_from_file(player.track->path);
		LOG_DEBUG("deck_load_folder set cues.load_from_file ok");
	}
	else
	{
		nav_state.files_present = false;
	}
}

void Deck::next_file(struct Sc1000* engine, struct ScSettings* settings)
{
	LOG_DEBUG("deck %d next_file called, nav_state.files_present=%d, nav_state.file_idx=%d, source=%d",
	          deck_no, nav_state.files_present, nav_state.file_idx, static_cast<int>(player.input.source));

	if (!nav_state.files_present) return;

	if (nav_state.file_idx == -1)
	{
		// At loop, go to first file
		nav_state.file_idx = 0;
		player.input.source = sc::PlaybackSource::File;
		ScFile* file = playlist->get_file(nav_state.folder_idx, 0);
		if (file != nullptr)
		{
			load_track_internal(this, track_acquire_by_import(importer.c_str(), file->full_path.c_str()), settings);
			LOG_DEBUG("deck %d next_file: loaded file 0", deck_no);
		}
		else
		{
			LOG_DEBUG("deck %d next_file: no file at folder %zu index 0", deck_no, nav_state.folder_idx);
		}
	}
	else if (playlist->has_next_file(nav_state.folder_idx, static_cast<size_t>(nav_state.file_idx)))
	{
		nav_state.file_idx++;
		ScFile* file = playlist->get_file(nav_state.folder_idx, static_cast<size_t>(nav_state.file_idx));
		if (file != nullptr)
		{
			load_track_internal(this, track_acquire_by_import(importer.c_str(), file->full_path.c_str()), settings);
			LOG_DEBUG("deck %d next_file: loaded file %d", deck_no, nav_state.file_idx);
		}
	}
}

void Deck::prev_file(struct Sc1000* engine, struct ScSettings* settings)
{
	LOG_DEBUG("deck %d prev_file called, nav_state.files_present=%d, nav_state.file_idx=%d, source=%d",
	          deck_no, nav_state.files_present, nav_state.file_idx, static_cast<int>(player.input.source));

	if (!nav_state.files_present) return;

	if (nav_state.file_idx == -1)
	{
		// Already at loop, do nothing
		LOG_DEBUG("deck %d prev_file: already at loop, staying", deck_no);
		return;
	}
	else if (nav_state.file_idx == 0)
	{
		// At first file, go to loop if exists
		bool has_loop = engine->audio && engine->audio->has_loop(deck_no);
		LOG_DEBUG("deck %d prev_file: at file 0, has_loop=%d", deck_no, has_loop);
		if (has_loop)
		{
			goto_loop(engine, settings);
			LOG_DEBUG("deck %d prev_file: went to loop", deck_no);
		}
		// else: no loop, stay at position 0
	}
	else
	{
		// Normal prev behavior
		nav_state.file_idx--;
		player.input.source = sc::PlaybackSource::File;
		ScFile* file = playlist->get_file(nav_state.folder_idx, static_cast<size_t>(nav_state.file_idx));
		if (file != nullptr)
		{
			load_track_internal(this, track_acquire_by_import(importer.c_str(), file->full_path.c_str()), settings);
			LOG_DEBUG("deck %d prev_file: loaded file %d", deck_no, nav_state.file_idx);
		}
	}
}

void Deck::next_folder(struct Sc1000* engine, struct ScSettings* settings)
{
	if (!nav_state.files_present) return;

	// If at loop, stay at loop (folder change doesn't affect it)
	if (nav_state.file_idx == -1)
	{
		if (playlist->has_next_folder(nav_state.folder_idx))
		{
			nav_state.folder_idx++;
			LOG_DEBUG("Deck %d: next_folder to %zu (staying at loop)", deck_no, nav_state.folder_idx);
		}
		return;  // Stay at loop
	}

	// Normal folder navigation
	if (playlist->has_next_folder(nav_state.folder_idx))
	{
		nav_state.folder_idx++;
		nav_state.file_idx = 0;
		ScFile* file = playlist->get_file(nav_state.folder_idx, 0);
		load_track_internal(this, track_acquire_by_import(importer.c_str(), file->full_path.c_str()), settings);
		LOG_DEBUG("Deck %d: next_folder to %zu, file 0", deck_no, nav_state.folder_idx);
	}
}

void Deck::prev_folder(struct Sc1000* engine, struct ScSettings* settings)
{
	if (!nav_state.files_present) return;

	// If at loop, stay at loop (folder change doesn't affect it)
	if (nav_state.file_idx == -1)
	{
		if (playlist->has_prev_folder(nav_state.folder_idx))
		{
			nav_state.folder_idx--;
			LOG_DEBUG("Deck %d: prev_folder to %zu (staying at loop)", deck_no, nav_state.folder_idx);
		}
		return;  // Stay at loop
	}

	// Normal folder navigation
	if (playlist->has_prev_folder(nav_state.folder_idx))
	{
		nav_state.folder_idx--;
		nav_state.file_idx = 0;
		ScFile* file = playlist->get_file(nav_state.folder_idx, 0);
		load_track_internal(this, track_acquire_by_import(importer.c_str(), file->full_path.c_str()), settings);
		LOG_DEBUG("Deck %d: prev_folder to %zu, file 0", deck_no, nav_state.folder_idx);
	}
}

void Deck::random_file(struct Sc1000* engine, struct ScSettings* settings)
{
	if (nav_state.files_present)
	{
		unsigned int num_files = static_cast<unsigned int>(playlist->total_files());
		unsigned int r = static_cast<unsigned int>(rand()) % num_files;
		LOG_DEBUG("Deck %d: random_file %d/%d", deck_no, r, num_files);
		ScFile* file = playlist->get_file_at_index(r);
		if (file != nullptr)
		{
			// Random file exits loop mode
			player.input.source = sc::PlaybackSource::File;
			// We don't update nav_state.file_idx here since random doesn't fit folder navigation
			// Just load the track
			load_track_internal(this, track_acquire_by_import(importer.c_str(), file->full_path.c_str()), settings);
		}
	}
}

void Deck::record(struct Sc1000* engine)
{
	// Query current recording state and set appropriate request
	bool currently_recording = engine->audio && engine->audio->is_recording(deck_no);
	if (currently_recording) {
		player.input.record_stop = true;
	} else {
		player.input.record_start = true;
	}
}

bool Deck::recall_loop(struct ScSettings* settings)
{
	if (!loop_state.track || loop_state.track->length == 0)
	{
		return false;
	}

	track_acquire(loop_state.track);
	player.set_track(loop_state.track);

	player.input.seek_to = 0.0;
	player.input.position_offset = 0.0;
	player.input.stopped = false;  // Reset stopped state so scratching works immediately

	// Reset cap_touch to force re-detection and proper angle_offset recalculation
	// (Part of encoder glitch protection chain - see audio_engine.cpp:173)
	player.input.touched = false;

	// Reset encoder offset (we're seeking to position 0)
	encoder_state.offset = 0 - encoder_state.angle;

	return true;
}

bool Deck::has_loop() const
{
	return loop_state.track != nullptr && loop_state.track->length > 0;
}

void Deck::goto_loop(struct Sc1000* engine, struct ScSettings* settings)
{
	nav_state.file_idx = -1;
	player.input.source = sc::PlaybackSource::Loop;
	player.input.seek_to = 0.0;
	player.input.position_offset = 0.0;
	player.input.stopped = false;  // Reset stopped state so scratching works immediately

	// Reset cap_touch to force re-detection and proper angle_offset recalculation
	// (Part of encoder glitch protection chain - see audio_engine.cpp:173)
	player.input.touched = false;

	// Reset encoder offset (we're seeking to position 0)
	encoder_state.offset = 0 - encoder_state.angle;

	LOG_DEBUG("Deck %d: goto_loop", deck_no);
}

//
// Auto-cue mode implementation
//

int Deck::auto_cue_divisions() const
{
	switch (auto_cue_mode) {
		case AutoCueMode::Div4:  return 4;
		case AutoCueMode::Div8:  return 8;
		case AutoCueMode::Div16: return 16;
		case AutoCueMode::Div32: return 32;
		default:                 return 0;
	}
}

void Deck::cycle_auto_cue_mode()
{
	switch (auto_cue_mode) {
		case AutoCueMode::Off:   auto_cue_mode = AutoCueMode::Div4;  break;
		case AutoCueMode::Div4:  auto_cue_mode = AutoCueMode::Div8;  break;
		case AutoCueMode::Div8:  auto_cue_mode = AutoCueMode::Div16; break;
		case AutoCueMode::Div16: auto_cue_mode = AutoCueMode::Div32; break;
		case AutoCueMode::Div32: auto_cue_mode = AutoCueMode::Off;   break;
	}

	LOG_INFO("Deck %d: auto-cue mode = %d divisions", deck_no, auto_cue_divisions());
}


