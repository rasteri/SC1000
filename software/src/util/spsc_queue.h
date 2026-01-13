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
 * SPSC Queue wrapper
 *
 * Uses moodycamel::ReaderWriterQueue - a battle-tested lock-free
 * single-producer single-consumer queue.
 *
 * See deps/readerwriterqueue/ for the implementation.
 */

#pragma once

#include <readerwriterqueue/readerwriterqueue.h>

namespace sc {

// Alias for convenience
template<typename T, size_t MaxBlockSize = 512>
using SPSCQueue = moodycamel::ReaderWriterQueue<T, MaxBlockSize>;

} // namespace sc
