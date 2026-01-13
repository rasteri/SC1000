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
 * Structured logging system for SC1000
 *
 * Features:
 * - Console or file output based on startup flags
 * - RT-safe logging via lock-free queue
 * - Log levels (debug, info, warn, error)
 * - Separate stats output flag for FPS/DSP metrics
 * - File/line information in debug mode
 * - Type-safe formatting via tinyformat
 */

#pragma once

#include <cstdarg>
#include <string>
#include "tinyformat.h"

namespace sc {
namespace log {

// Log levels in order of severity
enum class Level {
    LVL_DEBUG,   // Named to avoid conflict with DEBUG macro
    LVL_INFO,
    LVL_WARN,
    LVL_ERROR
};

// Configuration for log initialization
struct Config {
    bool use_file = false;              // Output to file instead of console
    const char* file_path = nullptr;    // Custom file path (nullptr = default)
    Level min_level = Level::LVL_INFO;  // Minimum level to log
    bool show_stats = false;            // Enable FPS/DSP stats output
};

// Initialize logging system (call once at startup)
void init(const Config& config);

// Shutdown logging system (call at exit)
void shutdown();

// Standard logging (for non-RT threads)
// These may block and allocate
void log(Level level, const char* file, int line, const char* fmt, ...);
void vlog(Level level, const char* file, int line, const char* fmt, va_list args);

// RT-safe logging (for realtime thread)
// These are lock-free and non-blocking
// Messages are queued and flushed from the main thread
void log_rt(Level level, const char* fmt, ...);
void vlog_rt(Level level, const char* fmt, va_list args);

// Stats output (FPS, DSP load, etc.)
// Only outputs if show_stats was enabled in config
void stats(const char* fmt, ...);

// Flush RT log queue (call from main thread periodically)
void flush_rt_logs();

// Check if a level would be logged (useful to avoid expensive formatting)
bool would_log(Level level);

// Check if stats are enabled
bool stats_enabled();

// Get level name as string
const char* level_name(Level level);

// Low-level write function (used by template functions below)
void write_message(Level level, const char* file, int line, const std::string& message);
void write_message_rt(Level level, const std::string& message);
void write_stats(const std::string& message);

// Type-safe logging using tinyformat
// These are the preferred interface for new code

template<typename... Args>
void log_debug(const char* file, int line, const char* fmt, const Args&... args) {
    if (!would_log(Level::LVL_DEBUG)) return;
    write_message(Level::LVL_DEBUG, file, line, tfm::format(fmt, args...));
}

template<typename... Args>
void log_info(const char* file, int line, const char* fmt, const Args&... args) {
    if (!would_log(Level::LVL_INFO)) return;
    write_message(Level::LVL_INFO, file, line, tfm::format(fmt, args...));
}

template<typename... Args>
void log_warn(const char* file, int line, const char* fmt, const Args&... args) {
    if (!would_log(Level::LVL_WARN)) return;
    write_message(Level::LVL_WARN, file, line, tfm::format(fmt, args...));
}

template<typename... Args>
void log_error(const char* file, int line, const char* fmt, const Args&... args) {
    if (!would_log(Level::LVL_ERROR)) return;
    write_message(Level::LVL_ERROR, file, line, tfm::format(fmt, args...));
}

// RT-safe type-safe logging
template<typename... Args>
void log_debug_rt(const char* fmt, const Args&... args) {
    write_message_rt(Level::LVL_DEBUG, tfm::format(fmt, args...));
}

template<typename... Args>
void log_info_rt(const char* fmt, const Args&... args) {
    write_message_rt(Level::LVL_INFO, tfm::format(fmt, args...));
}

template<typename... Args>
void log_warn_rt(const char* fmt, const Args&... args) {
    write_message_rt(Level::LVL_WARN, tfm::format(fmt, args...));
}

template<typename... Args>
void log_error_rt(const char* fmt, const Args&... args) {
    write_message_rt(Level::LVL_ERROR, tfm::format(fmt, args...));
}

// Type-safe stats output
template<typename... Args>
void stats_fmt(const char* fmt, const Args&... args) {
    if (!stats_enabled()) return;
    write_stats(tfm::format(fmt, args...));
}

} // namespace log
} // namespace sc

// Type-safe macros (preferred for new code)
#define LOG_DEBUG(...) ::sc::log::log_debug(__FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...)  ::sc::log::log_info(__FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...)  ::sc::log::log_warn(__FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...) ::sc::log::log_error(__FILE__, __LINE__, __VA_ARGS__)

// RT-safe type-safe macros
#define LOG_RT_DEBUG(...) ::sc::log::log_debug_rt(__VA_ARGS__)
#define LOG_RT_INFO(...)  ::sc::log::log_info_rt(__VA_ARGS__)
#define LOG_RT_WARN(...)  ::sc::log::log_warn_rt(__VA_ARGS__)
#define LOG_RT_ERROR(...) ::sc::log::log_error_rt(__VA_ARGS__)

// Type-safe stats macro
#define LOG_STATS(...) ::sc::log::stats_fmt(__VA_ARGS__)

// Legacy printf-style macros (for compatibility)
#define SC_LOG_DEBUG(...) ::sc::log::log(::sc::log::Level::LVL_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define SC_LOG_INFO(...)  ::sc::log::log(::sc::log::Level::LVL_INFO,  __FILE__, __LINE__, __VA_ARGS__)
#define SC_LOG_WARN(...)  ::sc::log::log(::sc::log::Level::LVL_WARN,  __FILE__, __LINE__, __VA_ARGS__)
#define SC_LOG_ERROR(...) ::sc::log::log(::sc::log::Level::LVL_ERROR, __FILE__, __LINE__, __VA_ARGS__)

// Legacy RT-safe macros
#define SC_LOG_RT_DEBUG(...) ::sc::log::log_rt(::sc::log::Level::LVL_DEBUG, __VA_ARGS__)
#define SC_LOG_RT_INFO(...)  ::sc::log::log_rt(::sc::log::Level::LVL_INFO,  __VA_ARGS__)
#define SC_LOG_RT_WARN(...)  ::sc::log::log_rt(::sc::log::Level::LVL_WARN,  __VA_ARGS__)
#define SC_LOG_RT_ERROR(...) ::sc::log::log_rt(::sc::log::Level::LVL_ERROR, __VA_ARGS__)

// Legacy stats macro
#define SC_LOG_STATS(...) ::sc::log::stats(__VA_ARGS__)
