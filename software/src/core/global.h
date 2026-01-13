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


#pragma once

#include "sc_settings.h"
#include "sc_input.h"
#include "sc1000.h"
#include "../thread/realtime.h"

#define DEVICE_CHANNELS 2
#define TARGET_SAMPLE_RATE 48000                 // 48khz
#define TARGET_SAMPLE_FORMAT SND_PCM_FORMAT_S16  // 16-bit signed little-endian format

#define DEFAULT_IMPORTER "/root/sc1000-import"

extern struct Sc1000      g_sc1000_engine;
extern struct Rt          g_rt;