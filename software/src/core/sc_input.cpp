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

// SC1000 Input Thread Coordinator
// Manages the input thread and coordinates hardware and MIDI input layers
// This file should be hardware-agnostic - all SC1000-specific code is in sc_hardware.cpp

#include <cstdlib>
#include <unistd.h>
#include <sys/time.h>
#include <ctime>
#include <pthread.h>

#include "../platform/sc_hardware.h"
#include "../input/midi_input.h"

#include "global.h"
#include "sc_input.h"
#include "../util/log.h"

namespace sc {
namespace input {

using namespace sc::platform;

/*
 * InputContext holds all mutable state for the input thread.
 * Combines hardware and MIDI contexts into a single coordinator.
 */
struct InputContext {
    // Hardware layer (polymorphic - SC1000, motorized platter, etc.)
    std::unique_ptr<HardwareInput> hardware;

    // MIDI input layer (controllers, events)
    MidiContext midi;
};

// Singleton input context
static InputContext g_input_ctx;

// Thread control
static volatile bool g_input_running = true;
static pthread_t g_input_thread_handle;

void* run_sc_input_thread(Sc1000* engine)
{
    ScSettings* settings = engine->settings.get();
    MidiContext* midi_ctx = &g_input_ctx.midi;

    // Create and initialize hardware layer
    g_input_ctx.hardware = create_hardware();
    g_input_ctx.hardware->init(engine);

    // Initialize MIDI layer
    init_midi(midi_ctx);

    // Seed random number generator (used for random file selection)
    srand(static_cast<unsigned int>(time(nullptr)));

    struct timeval tv;
    time_t last_time = 0;
    unsigned int frame_count = 0;
    unsigned int second_count = 0;

    // Give hardware time to stabilize
    sleep(2);

    while (g_input_running)
    {
        frame_count++;

        // Once per second: log stats, poll for new MIDI devices
        gettimeofday(&tv, nullptr);
        if (tv.tv_sec != last_time)
        {
            last_time = tv.tv_sec;
            // Log hardware and DSP stats
            LOG_STATS("FPS: %06u - ", frame_count);
            g_input_ctx.hardware->log_stats(engine);
            frame_count = 0;

            // Debug: list connected MIDI controllers
            for (const auto& controller : midi_ctx->controllers)
            {
                LOG_DEBUG("MIDI : %s", controller->port_name());
            }

            // Poll for new MIDI devices after init delay
            if (second_count < settings->midi_init_delay)
            {
                second_count++;
            }
            else if (second_count == settings->midi_init_delay)
            {
                poll_midi_devices(midi_ctx, engine);
                second_count = 999;  // Don't poll again
            }
        }

        // Poll hardware inputs (PIC, GPIO, encoder)
        g_input_ctx.hardware->poll(engine);

        // Process MIDI events from the lock-free queue
        process_midi_events(engine);

        // Rate limit input loop
        usleep(settings->update_rate);
    }

    return nullptr;
}

void* sc_input_thread(void* ptr)
{
    (void)ptr;  // Unused
    return run_sc_input_thread(&g_sc1000_engine);
}

} // namespace input
} // namespace sc


////////////////////////////////////////////////////////////////////////////////////////////////////////////
// C interface for thread management

void start_sc_input_thread()
{
    LOG_INFO("Starting input thread");
    sc::input::g_input_running = true;

    int result = pthread_create(&sc::input::g_input_thread_handle, nullptr, sc::input::sc_input_thread, nullptr);
    if (result)
    {
        LOG_ERROR("pthread_create failed: %d", result);
        exit(EXIT_FAILURE);
    }
}

void stop_sc_input_thread()
{
    LOG_INFO("Stopping input thread");
    sc::input::g_input_running = false;
    pthread_join(sc::input::g_input_thread_handle, nullptr);
    LOG_INFO("Input thread stopped");
}
