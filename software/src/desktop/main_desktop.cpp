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


/*
 * SC1000 Desktop Test Application
 *
 * A simple terminal-based test application that simulates the SC1000
 * hardware inputs via keyboard controls. This allows testing the software
 * without actual hardware.
 *
 * Currently provides:
 * - Mock encoder (platter position) via arrow keys
 * - Mock fader (crossfader) via arrow keys
 * - Mock cap touch via spacebar
 * - Mock buttons via number keys
 *
 * Future: Could connect to actual SC1000 audio engine for full testing.
 */

#include "platform_desktop.h"

#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <unistd.h>

using namespace sc::desktop;

static DesktopPlatformState g_state;

static void signal_handler(int sig)
{
    (void)sig;
    g_state.running = false;
}

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    // Set up signal handlers for clean exit
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    printf("SC1000 Desktop Test Application\n");
    printf("================================\n\n");
    printf("This is a mock platform for testing SC1000 controls.\n");
    printf("Press any key to start (or Q to quit)...\n\n");
    print_controls();

    // Wait for initial keypress
    getchar();

    // Initialize terminal for raw input
    terminal_init();

    // Initial draw
    draw_state(&g_state);

    // Main loop
    while (g_state.running) {
        // Process keyboard input
        if (!process_keyboard(&g_state)) {
            break;
        }

        // Update display if state changed
        draw_state(&g_state);

        // Small sleep to avoid burning CPU
        usleep(10000); // 10ms = 100Hz update rate
    }

    // Cleanup
    terminal_cleanup();

    printf("Goodbye!\n");
    return 0;
}
