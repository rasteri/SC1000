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

// MIDI Input Layer - Implementation
// Generic MIDI device enumeration and event processing

#include "midi_input.h"
#include "midi_event.h"
#include "controller.h"
#include "../platform/midi.h"
#include "../core/sc1000.h"
#include "../core/sc_settings.h"
#include "../core/global.h"
#include "../control/actions.h"
#include "../util/log.h"

#include <cstring>

namespace sc {
namespace input {

using sc::control::dispatch_event;

void init_midi(MidiContext* ctx)
{
    ctx->controllers.clear();
    ctx->device_count = 0;
    ctx->old_device_count = 0;
}

void poll_midi_devices(MidiContext* ctx, Sc1000* engine)
{
    // Enumerate MIDI devices
    ctx->device_count = listdev("rawmidi", ctx->device_names);

    // If there are more MIDI devices than last time, add them
    if (ctx->device_count > ctx->old_device_count)
    {
        // Search to see which devices we've already added
        for (int devc = 0; devc < ctx->device_count; devc++)
        {
            bool already_added = false;

            for (const auto& controller : ctx->controllers)
            {
                if (strcmp(ctx->device_names[devc], controller->port_name()) == 0) {
                    already_added = true;
                    break;
                }
            }

            if (!already_added)
            {
                auto controller = create_midi_controller(&g_rt, ctx->device_names[devc]);
                if (controller)
                {
                    LOG_INFO("Adding MIDI device %zu - %s", ctx->controllers.size(), ctx->device_names[devc]);
                    controller_add_deck(controller.get(), &engine->beat_deck);
                    controller_add_deck(controller.get(), &engine->scratch_deck);
                    ctx->controllers.push_back(std::move(controller));
                }
            }
        }

        ctx->old_device_count = ctx->device_count;
    }
}

void process_midi_events(Sc1000* engine)
{
    ScSettings* settings = engine->settings.get();

    // Process MIDI events from the lock-free queue
    unsigned char midi_bytes[3];
    int midi_shifted;
    while (midi_event_queue_pop(midi_bytes, &midi_shifted)) {
        EventType edge = midi_shifted ? BUTTON_PRESSED_SHIFTED : BUTTON_PRESSED;

        // Create MidiCommand from bytes and use registry lookup
        MidiCommand cmd = MidiCommand::from_bytes(midi_bytes);
        cmd.normalize();  // Note-on with velocity 0 becomes note-off

        Mapping* midi_map = engine->mappings.find_midi(cmd, edge);
        if (midi_map != nullptr) {
            LOG_DEBUG("MIDI Mapping found: action=%d deck=%d param=%d",
                     midi_map->action_type, midi_map->deck_no, midi_map->parameter);
            dispatch_event(midi_map, midi_bytes, engine, settings, engine->input_state);
        } else {
            LOG_DEBUG("MIDI no Mapping for [%02X %02X %02X] shifted=%d",
                     midi_bytes[0], midi_bytes[1], midi_bytes[2], midi_shifted);
        }
    }
}

} // namespace input
} // namespace sc
