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


// Desktop platform mock implementation
#include "platform_desktop.h"

#include <cstdio>
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
#include <cstring>

namespace sc {
namespace desktop {

static struct termios g_orig_termios;
static bool g_terminal_initialized = false;

void terminal_init()
{
    if (g_terminal_initialized) return;

    // Save original terminal settings
    tcgetattr(STDIN_FILENO, &g_orig_termios);

    // Set raw mode (no echo, no line buffering)
    struct termios raw = g_orig_termios;
    raw.c_lflag &= static_cast<tcflag_t>(~(ECHO | ICANON));
    raw.c_cc[VMIN] = 0;   // Non-blocking read
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

    // Hide cursor
    printf("\033[?25l");
    fflush(stdout);

    g_terminal_initialized = true;
}

void terminal_cleanup()
{
    if (!g_terminal_initialized) return;

    // Show cursor
    printf("\033[?25h");
    // Clear screen
    printf("\033[2J\033[H");
    fflush(stdout);

    // Restore terminal settings
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_orig_termios);

    g_terminal_initialized = false;
}

// Check if a key is available
static bool key_available()
{
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    struct timeval tv = {0, 0};
    return select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv) > 0;
}

// Read a key (returns 0 if none available)
static int read_key()
{
    if (!key_available()) return 0;

    char c;
    if (read(STDIN_FILENO, &c, 1) != 1) return 0;

    // Handle escape sequences (arrow keys)
    if (c == '\033') {
        char seq[2];
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\033';
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\033';

        if (seq[0] == '[') {
            switch (seq[1]) {
                case 'A': return 1001; // Up arrow
                case 'B': return 1002; // Down arrow
                case 'C': return 1003; // Right arrow
                case 'D': return 1004; // Left arrow
            }
        }
        return '\033';
    }

    return c;
}

bool process_keyboard(DesktopPlatformState* state)
{
    int key = read_key();
    if (key == 0) return true;

    state->display_dirty = true;

    switch (key) {
        // Quit
        case 'q':
        case 'Q':
            state->running = false;
            return false;

        // Encoder control (left/right arrows)
        case 1003: // Right arrow - increase angle
            state->encoder_angle = (state->encoder_angle.load() + 100) % 4096;
            break;
        case 1004: // Left arrow - decrease angle
            state->encoder_angle = (state->encoder_angle.load() - 100 + 4096) % 4096;
            break;

        // Fine encoder control
        case '.': // Fine right
            state->encoder_angle = (state->encoder_angle.load() + 10) % 4096;
            break;
        case ',': // Fine left
            state->encoder_angle = (state->encoder_angle.load() - 10 + 4096) % 4096;
            break;

        // Fader control (up/down arrows)
        case 1001: // Up arrow - increase fader
            {
                int fader = state->fader_position.load() + 200;
                if (fader > 4095) fader = 4095;
                state->fader_position = static_cast<uint16_t>(fader);
            }
            break;
        case 1002: // Down arrow - decrease fader
            {
                int fader = state->fader_position.load() - 200;
                if (fader < 0) fader = 0;
                state->fader_position = static_cast<uint16_t>(fader);
            }
            break;

        // Cap touch toggle
        case ' ':
            state->cap_touch = !state->cap_touch.load();
            break;

        // Buttons 1-8
        case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8':
            {
                int btn = key - '1';
                uint16_t mask = static_cast<uint16_t>(1 << btn);
                state->button_state = state->button_state.load() ^ mask;
            }
            break;

        // Reset all
        case 'r':
        case 'R':
            state->encoder_angle = 0;
            state->fader_position = 2048;
            state->cap_touch = false;
            state->button_state = 0;
            break;

        default:
            state->display_dirty = false;
            break;
    }

    return true;
}

void draw_state(const DesktopPlatformState* state)
{
    if (!state->display_dirty.load()) return;

    // Clear screen and move cursor to top
    printf("\033[2J\033[H");

    printf("SC1000 Desktop Test Application\n");
    printf("================================\n\n");

    // Encoder display (as a simple bar)
    int angle = state->encoder_angle.load();
    int angle_bar = angle * 40 / 4096;
    printf("Encoder: [");
    for (int i = 0; i < 40; i++) {
        printf("%c", (i == angle_bar) ? '|' : '-');
    }
    printf("] %4d\n\n", angle);

    // Fader display
    int fader = state->fader_position.load();
    int fader_bar = fader * 40 / 4096;
    printf("Fader:   [");
    for (int i = 0; i < 40; i++) {
        printf("%c", (i == fader_bar) ? '|' : '-');
    }
    printf("] %4d\n\n", fader);

    // Cap touch
    printf("Touch:   %s\n\n", state->cap_touch.load() ? "[TOUCHED]" : "[-------]");

    // Buttons
    uint16_t buttons = state->button_state.load();
    printf("Buttons: ");
    for (int i = 0; i < 8; i++) {
        printf("%c ", (buttons & (1 << i)) ? '1' + i : '.');
    }
    printf("\n\n");

    // Controls help
    printf("Controls:\n");
    printf("  Left/Right arrows : Encoder (coarse)\n");
    printf("  ,/.               : Encoder (fine)\n");
    printf("  Up/Down arrows    : Fader\n");
    printf("  Space             : Toggle cap touch\n");
    printf("  1-8               : Toggle buttons\n");
    printf("  R                 : Reset all\n");
    printf("  Q                 : Quit\n");

    fflush(stdout);
}

void print_controls()
{
    printf("\nSC1000 Desktop Test - Controls:\n");
    printf("  Left/Right arrows : Encoder (coarse)\n");
    printf("  ,/.               : Encoder (fine)\n");
    printf("  Up/Down arrows    : Fader\n");
    printf("  Space             : Toggle cap touch\n");
    printf("  1-8               : Toggle buttons\n");
    printf("  R                 : Reset all\n");
    printf("  Q                 : Quit\n\n");
}

} // namespace desktop
} // namespace sc
