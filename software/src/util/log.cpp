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
 * Structured logging implementation
 */

#include "log.h"
#include "spsc_queue.h"

#include <cstdio>
#include <cstring>
#include <ctime>
#include <atomic>

namespace sc {
namespace log {

// Maximum message length for RT logging
static constexpr size_t RT_MSG_MAX = 256;

// RT log message structure (fixed size for lock-free queue)
struct RTLogMessage {
    Level level;
    char message[RT_MSG_MAX];
};

// Global state
static Config g_config;
static FILE* g_output = nullptr;
static bool g_initialized = false;
static std::atomic<bool> g_shutdown{false};

// RT log queue (single producer = RT thread, single consumer = main thread)
static SPSCQueue<RTLogMessage, 256>* g_rt_queue = nullptr;

// Level names
static const char* LEVEL_NAMES[] = {
    "DEBUG",   // LVL_DEBUG
    "INFO",    // LVL_INFO
    "WARN",    // LVL_WARN
    "ERROR"    // LVL_ERROR
};

const char* level_name(Level level) {
    return LEVEL_NAMES[static_cast<int>(level)];
}

bool would_log(Level level) {
    return g_initialized && level >= g_config.min_level;
}

bool stats_enabled() {
    return g_initialized && g_config.show_stats;
}

void init(const Config& config) {
    g_config = config;
    g_shutdown = false;

    // Open output file or use stderr
    if (config.use_file) {
        const char* path = config.file_path;
        if (path == nullptr) {
            path = "/media/sda/sc1000.log";
        }
        g_output = fopen(path, "a");
        if (g_output == nullptr) {
            // Fall back to stderr if file open fails
            g_output = stderr;
            fprintf(stderr, "Warning: Could not open log file '%s', using stderr\n", path);
        } else {
            // Write header
            time_t now = time(nullptr);
            fprintf(g_output, "\n=== SC1000 Log Started: %s", ctime(&now));
            fflush(g_output);
        }
    } else {
        g_output = stderr;
    }

    // Create RT log queue
    g_rt_queue = new SPSCQueue<RTLogMessage, 256>(1024);

    g_initialized = true;
}

void shutdown() {
    if (!g_initialized) return;

    g_shutdown = true;

    // Flush any remaining RT logs
    flush_rt_logs();

    // Close file if we opened one
    if (g_config.use_file && g_output != nullptr && g_output != stderr) {
        time_t now = time(nullptr);
        fprintf(g_output, "=== SC1000 Log Ended: %s\n", ctime(&now));
        fclose(g_output);
    }

    g_output = nullptr;

    // Clean up queue
    delete g_rt_queue;
    g_rt_queue = nullptr;

    g_initialized = false;
}

static void write_log(Level level, const char* file, int line, const char* message) {
    if (g_output == nullptr) return;

    // Get timestamp
    time_t now = time(nullptr);
    struct tm* tm_info = localtime(&now);
    char time_buf[20];
    strftime(time_buf, sizeof(time_buf), "%H:%M:%S", tm_info);

    // Format output
    if (file != nullptr && level == Level::LVL_DEBUG) {
        // Include file/line for debug messages
        const char* basename = strrchr(file, '/');
        if (basename) basename++; else basename = file;
        fprintf(g_output, "[%s] %s %s:%d: %s\n",
                time_buf, level_name(level), basename, line, message);
    } else {
        fprintf(g_output, "[%s] %s: %s\n",
                time_buf, level_name(level), message);
    }

    fflush(g_output);
}

void vlog(Level level, const char* file, int line, const char* fmt, va_list args) {
    if (!would_log(level)) return;

    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), fmt, args);

    write_log(level, file, line, buffer);
}

void log(Level level, const char* file, int line, const char* fmt, ...) {
    if (!would_log(level)) return;

    va_list args;
    va_start(args, fmt);
    vlog(level, file, line, fmt, args);
    va_end(args);
}

void vlog_rt(Level level, const char* fmt, va_list args) {
    if (!g_initialized || g_rt_queue == nullptr) return;
    if (level < g_config.min_level) return;

    RTLogMessage msg;
    msg.level = level;
    vsnprintf(msg.message, RT_MSG_MAX, fmt, args);

    // Try to enqueue (non-blocking, may fail if queue is full)
    g_rt_queue->try_enqueue(msg);
}

void log_rt(Level level, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vlog_rt(level, fmt, args);
    va_end(args);
}

void stats(const char* fmt, ...) {
    if (!stats_enabled()) return;

    va_list args;
    va_start(args, fmt);

    char buffer[256];
    vsnprintf(buffer, sizeof(buffer), fmt, args);

    // Stats go directly to output without timestamp/level prefix
    if (g_output != nullptr) {
        fprintf(g_output, "%s", buffer);
        fflush(g_output);
    }

    va_end(args);
}

void flush_rt_logs() {
    if (!g_initialized || g_rt_queue == nullptr) return;

    RTLogMessage msg;
    while (g_rt_queue->try_dequeue(msg)) {
        write_log(msg.level, nullptr, 0, msg.message);
    }
}

// Type-safe write functions (used by template functions in header)

void write_message(Level level, const char* file, int line, const std::string& message) {
    if (!would_log(level)) return;
    write_log(level, file, line, message.c_str());
}

void write_message_rt(Level level, const std::string& message) {
    if (!g_initialized || g_rt_queue == nullptr) return;
    if (level < g_config.min_level) return;

    RTLogMessage msg;
    msg.level = level;
    strncpy(msg.message, message.c_str(), RT_MSG_MAX - 1);
    msg.message[RT_MSG_MAX - 1] = '\0';

    // Try to enqueue (non-blocking, may fail if queue is full)
    g_rt_queue->try_enqueue(msg);
}

void write_stats(const std::string& message) {
    if (!stats_enabled()) return;

    if (g_output != nullptr) {
        fprintf(g_output, "%s", message.c_str());
        fflush(g_output);
    }
}

} // namespace log
} // namespace sc
