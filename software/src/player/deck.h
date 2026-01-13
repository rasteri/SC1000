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

#pragma once

#include <cstdint>
#include <memory>
#include <optional>

#include "cues.h"
#include "deck_state.h"
#include "player.h"

// Auto-cue mode for automatic track division
enum class AutoCueMode {
   Off,     // Normal manual cue behavior
   Div4,    // 4 equal divisions
   Div8,    // 8 equal divisions
   Div16,   // 16 equal divisions
   Div32    // 32 equal divisions
};

#ifdef __cplusplus
#include "playlist.h"
#else
struct Playlist;
typedef struct Playlist Playlist;
#endif

struct ScSettings;
struct Track;
struct Sc1000;

struct Deck
{
   std::string importer;

   struct Player player;
   Cues cues;

   // Punch-in point (nullopt = not punching)
   std::optional<double> punch;

   // If a shift modifier has been pressed recently
   bool shifted;

   // === Grouped state ===
   NavigationState nav_state;
   EncoderState encoder_state;
   LoopState loop_state;

   // Auto-cue mode (divides track into equal parts)
   AutoCueMode auto_cue_mode = AutoCueMode::Off;

   // Playlist (owned)
   std::unique_ptr<Playlist> playlist;
   int deck_no;  // 0 = beat, 1 = scratch

#ifdef __cplusplus
   // C++ member functions
   ~Deck();  // Destructor defined in .cpp where Playlist is complete
   int init(struct ScSettings* settings);
   void clear();
   bool is_locked(struct Sc1000* engine) const;
   void recue(struct Sc1000* engine);
   void clone(const Deck& from, struct Sc1000* engine);
   void unset_cue(unsigned int label);
   void cue(unsigned int label, struct Sc1000* engine);
   void punch_in(unsigned int label, struct Sc1000* engine);
   void punch_out(struct Sc1000* engine);
   void load_folder(const char* folder_name);
   void next_file(struct Sc1000* engine, struct ScSettings* settings);
   void prev_file(struct Sc1000* engine, struct ScSettings* settings);
   void next_folder(struct Sc1000* engine, struct ScSettings* settings);
   void prev_folder(struct Sc1000* engine, struct ScSettings* settings);
   void random_file(struct Sc1000* engine, struct ScSettings* settings);
   void record(struct Sc1000* engine);
   bool recall_loop(struct ScSettings* settings);
   bool has_loop() const;

   // Loop navigation helpers
   bool is_at_loop() const { return nav_state.is_at_loop(); }
   void goto_loop(struct Sc1000* engine, struct ScSettings* settings);

   // Auto-cue mode
   void cycle_auto_cue_mode();
   void reset_auto_cue_mode() { auto_cue_mode = AutoCueMode::Off; }
   int auto_cue_divisions() const;
#endif
};


