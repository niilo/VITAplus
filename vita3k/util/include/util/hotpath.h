// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program.  If not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#pragma once

#include <cstddef>
#include <cstdint>

// Hot-path counters. These count how often the emulator's own choke points run,
// which is a different question from where the host CPU time goes: simpleperf
// answers that, and it cannot name the JIT output at all.
//
// Everything here is off by default. When it is off, every entry point returns
// on a single relaxed load and does no work. See
// .scratch/cpu-hotpath-counters/spec.md for why per-instruction guest counting
// is not done instead.
namespace hotpath {

// Set once at start-up from the config. `time` adds self-time to the HLE
// counters, and is separate because it costs two clock reads per HLE call.
void set_enabled(bool counts, bool time);
bool enabled();
bool time_enabled();

// Zero every counter. Called on each game start so a session does not carry the
// previous one's totals.
void reset();

// HLE import calls, one counter per NID. Call this at the top of the dispatch
// and add_hle_time at the bottom when timing is on.
void count_hle_call(uint32_t nid);
void add_hle_time(uint32_t nid, uint64_t nanoseconds);

// Instructions handed to the dynarmic translator. This counts translation, not
// execution: it stops once a block is compiled.
void count_translated_instruction();

// JIT code cache invalidation, with the byte length of the range.
void count_cache_invalidation(size_t length);

// Page-table callback entries, which are the accesses the page table could not
// resolve. Compared against the total this gives the fast-path miss rate.
void count_page_table_read();
void count_page_table_write();

// A guest access that looked invalid and turned out to be valid, which is the
// lock-free validity race being caught in the act.
void count_invalid_access_recovery();

// Write one interval of rows to `hle.csv` and `jit.csv` through perf_log.
// Called by the display's vblank thread once per interval; not for callers.
void dump_interval();

} // namespace hotpath