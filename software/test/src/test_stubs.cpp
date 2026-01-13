/*
 * Copyright (C) 2024-2026 Niklas Klügel <lodsb@lodsb.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, as published by the Free Software Foundation.
 */

// Stubs for hardware-dependent functions in test mode

#include "core/sc1000.h"
#include "core/sc_settings.h"
#include "thread/rig.h"
#include "thread/realtime.h"
#include "util/status.h"
#include "util/external.h"
#include <cstdarg>
#include <cstdio>

// Stub for ALSA audio backend creation
std::unique_ptr<AudioHardware> alsa_create(Sc1000* engine, ScSettings* settings)
{
    // In test mode, audio backend is provided by TestAudioBackend
    return nullptr;
}

void alsa_clear_config_cache()
{
    // No-op in test mode
}

// Stub for realtime thread
int Rt::set_engine(Sc1000* engine)
{
    this->engine = engine;
    return 0;
}

void rt_not_allowed()
{
    // In test mode, we allow all operations from any thread
}

int rt_global_init()
{
    return 0;
}

// Stub for status display
const char* status()
{
    return "";
}

int status_level()
{
    return 0;
}

void status_set(int level, const char* s)
{
    (void)level;
    (void)s;
}

void status_printf(int level, const char* format, ...)
{
    // Optionally print to console for debugging
#ifdef TEST_VERBOSE
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
#endif
    (void)level;
    (void)format;
}

// Stub for rig thread - global instance
struct Rig g_rig;

int Rig::init()
{
    return 0;
}

void Rig::clear()
{
}

void Rig::post_track(Track* t)
{
    // No-op in test mode - tracks are managed directly
    (void)t;
}

void Rig::remove_track(Track* t)
{
    // No-op in test mode
    (void)t;
}

void Rig::acquire_lock()
{
}

void Rig::release_lock()
{
}

// Stub for external process spawning
pid_t fork_pipe(int* fd, const char* path, char* arg, ...)
{
    (void)path;
    (void)arg;
    *fd = -1;
    return -1;
}

pid_t fork_pipe_nb(int* fd, const char* path, char* arg, ...)
{
    // In test mode, we don't spawn external importers
    // Test tracks are generated programmatically
    (void)path;
    (void)arg;
    *fd = -1;
    return -1;
}

void rb_reset(struct Rb* rb)
{
    rb->len = 0;
}

ssize_t get_line(int fd, struct Rb* rb, char** string)
{
    (void)fd;
    (void)rb;
    (void)string;
    return -1;
}
