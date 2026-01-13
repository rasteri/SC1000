# SC1000 Future Features

## Audio Processing

### RIAA Scratch Emulation
Simulate the frequency response mismatch that occurs when scratching real vinyl.

**Background:** Vinyl is recorded with RIAA pre-emphasis and the phono preamp applies the inverse curve during playback. At 1x speed this nets to flat response, but when scratching at variable speeds the RIAA de-emphasis is applied to frequency-shifted audio, creating a characteristic coloration:
- At faster speeds: more bass, earlier treble rolloff ("wooly" sound)
- At slower speeds: less bass, brighter/thinner sound

**RIAA corner frequencies:**
- Bass transition: 500.5 Hz (318μs time constant)
- Treble rolloff: 2122 Hz (75μs time constant)

**Implementation approach:**
- Two cascaded zero-delay SVF shelving filters
- Cutoff frequencies modulated by playback speed: `corner_hz * speed`
- At 1x speed: neutral/bypass
- Reference: [stmlib SVF](https://github.com/pichenettes/stmlib/blob/master/dsp/filter.h) (Cytomic/Andy Simper approach)

**Note:** No known DJ software implements this effect explicitly - could be unique.

---

### Effects Chain
Add filter/delay/reverb effects for creative scratching.

---

### ARM NEON SIMD
Implement proper NEON intrinsics for audio engine (currently relies on auto-vectorization).

Priority areas:
- Sinc interpolation convolution (16-tap × 2 decks)
- Sample mixing/output

---

### CV Clock Output
Add clock/trigger outputs that are synced to playback speed.

**Use case:** Sync external gear (sequencers, drum machines) to the scratch deck tempo.

**Research needed:**
- How to derive clock pulses from variable playback speed
- Should clock follow actual platter movement or sample position?
- Handling reverse playback (negative speed)
- Clock division/multiplication options (1/4, 1/8, 1/16 notes)
- PLL-style smoothing to avoid jittery clock from scratch movements

**Possible approaches:**
- Sample-position-based: emit pulse every N samples of audio played
- Time-based with speed scaling: base BPM × playback speed
- Beat-grid aligned (requires BPM detection)

---

## Features

### Auto-Slice Cue Markers
Automatic cue marker placement for sample slicing.

**UI concept:** Press cue markers 1 & 2 simultaneously to trigger auto-slice:
- First press: 8 evenly-spaced slices
- Second press: 16 slices
- Third press: 32 slices
- Fourth press: clear auto-slices

**Implementation notes:**
- Detect simultaneous button press in actions.cpp
- Calculate slice positions based on sample length
- Consider beat-grid alignment if BPM detection added later
- Store slice count state per deck

---

### BPM Detection
Automatic tempo detection for beat-grid alignment.

---

### More Cue Points
Expand beyond current cue point limit.

---

## Deployment

### Update Mechanism
Fix the device update mechanism and create shell scripts for proper deployment.

**Current state:**
- Binary can be copied to USB stick root as `xwax`
- Device startup script checks for USB binary and runs it instead of internal version

**Needed:**
- Shell script to build and package release binary
- Script to deploy to device via SSH/SCP
- Verify USB stick update mechanism still works with new binary
- Consider versioning and rollback mechanism

---

## Hardware

### Display Output
Track info display support (if hardware permits).

---

### Better Entropy Source
`srand(time(NULL))` in sc_input.cpp:813 - SoC starts up deterministically.

Consider using `/dev/urandom` or hardware RNG if available.

---

## Code Quality

### Action Dispatch Refactoring
Replace 20+ if/else chain in `perform_action_for_deck()` with function table lookup.

**Current:** Linear chain of `if (action_type == X) else if (action_type == Y)...`

**Target:** `action_handlers[action_type](deck, mapping, midi, engine, settings, state)`

See plan file: `~/.claude/plans/squishy-hatching-pony.md`

---

### Separate Runtime State from Mapping
Move `debounce` and `shifted_at_press` out of `mapping` struct into separate `ButtonState`.

**Rationale:** These are per-button runtime state, not static configuration.

---

## Documentation

### Porting Documentation
Document the architectural changes from `original_version/` to the current C++ codebase.

**Should cover:**
- File reorganization (flat → src/ subdirectories)
- C to C++ conversions (structs with methods, namespaces, RAII)
- New features added (loop recording, CV output, sinc interpolation)
- Removed/replaced code
- Build system changes (Makefile → CMake)

---

## Completed

- [x] Sinc interpolation (16-tap anti-aliased resampling)
- [x] MappingRegistry with O(1) lookup
- [x] MidiCommand type-safe wrapper
- [x] InputState class
- [x] RAII ownership patterns (unique_ptr, virtual interfaces)
- [x] Loop recording / memory sampler
- [x] Root directory CLI parameter
- [x] Remove C-style bool comparisons
- [x] Remove C interface wrappers from deck/player
- [x] Gate B / CV output
- [x] MIDI device testing
- [x] Loop functionality (auto-loop, loop roll)
