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

#include <util/hotpath.h>

#include <util/perf_log.h>

#include <nids/functions.h>

#include <fmt/format.h>

#include <atomic>
#include <cstdint>

namespace hotpath {

namespace {

// One entry per NID. Open addressing with linear probing, so count_hle_call
// takes no lock and allocates nothing. The size is a power of two so the
// modulo is a mask.
//
// A game resolves a few thousand NIDs, so a full table means the table is too
// small rather than that the game is unusual. Overflow is counted so a run
// cannot look complete when it is not.
constexpr size_t kTableSize = 1u << 13;
constexpr size_t kTableMask = kTableSize - 1;

struct NidSlot {
    // 0 means the slot is free. It is never a real NID: a NID of 0 is counted
    // as overflow instead, so a free slot and a counted NID cannot be confused.
    std::atomic<uint32_t> nid{ 0 };
    std::atomic<uint64_t> calls{ 0 };
    std::atomic<uint64_t> nanoseconds{ 0 };
};

NidSlot g_nid_slots[kTableSize];

std::atomic<uint64_t> g_overflow_calls{ 0 };
std::atomic<uint64_t> g_overflow_nanoseconds{ 0 };

std::atomic<uint64_t> g_translated_instructions{ 0 };
std::atomic<uint64_t> g_cache_invalidations{ 0 };
std::atomic<uint64_t> g_cache_invalidation_bytes{ 0 };
std::atomic<uint64_t> g_page_table_reads{ 0 };
std::atomic<uint64_t> g_page_table_writes{ 0 };
std::atomic<uint64_t> g_invalid_access_recoveries{ 0 };

std::atomic<bool> g_enabled{ false };
std::atomic<bool> g_time_enabled{ false };

// Knuth's multiplicative hash. NIDs are already hashes, so this only has to
// spread the low bits that the mask keeps.
inline size_t slot_for(uint32_t nid) {
    return (static_cast<uint32_t>(nid) * 2654435761u) >> 16 & kTableMask;
}

// Find the slot for nid, claiming a free one if needed. Returns nullptr when
// the table is full, and the caller counts an overflow.
NidSlot *find_slot(uint32_t nid) {
    if (nid == 0)
        return nullptr;

    const size_t start = slot_for(nid);
    for (size_t probe = 0; probe < kTableSize; probe++) {
        NidSlot &slot = g_nid_slots[(start + probe) & kTableMask];

        const uint32_t current = slot.nid.load(std::memory_order_relaxed);
        if (current == nid)
            return &slot;

        if (current == 0) {
            // Only one thread may claim a slot, so a failed exchange means
            // another thread took it. Re-read rather than assume it is ours.
            uint32_t expected = 0;
            if (slot.nid.compare_exchange_strong(expected, nid, std::memory_order_acq_rel, std::memory_order_relaxed))
                return &slot;
            if (expected == nid)
                return &slot;
        }
    }
    return nullptr;
}

const char *const kHleHeader = "nid,name,calls,ns";
const char *const kJitHeader = "counter,value";

} // namespace

void set_enabled(bool counts, bool time) {
    g_enabled.store(counts, std::memory_order_relaxed);
    // Timing needs the counters to be on, or it would add to nothing.
    g_time_enabled.store(counts && time, std::memory_order_relaxed);
}

bool enabled() {
    return g_enabled.load(std::memory_order_relaxed);
}

bool time_enabled() {
    return g_time_enabled.load(std::memory_order_relaxed);
}

void reset() {
    for (NidSlot &slot : g_nid_slots) {
        slot.nid.store(0, std::memory_order_relaxed);
        slot.calls.store(0, std::memory_order_relaxed);
        slot.nanoseconds.store(0, std::memory_order_relaxed);
    }
    g_overflow_calls.store(0, std::memory_order_relaxed);
    g_overflow_nanoseconds.store(0, std::memory_order_relaxed);
    g_translated_instructions.store(0, std::memory_order_relaxed);
    g_cache_invalidations.store(0, std::memory_order_relaxed);
    g_cache_invalidation_bytes.store(0, std::memory_order_relaxed);
    g_page_table_reads.store(0, std::memory_order_relaxed);
    g_page_table_writes.store(0, std::memory_order_relaxed);
    g_invalid_access_recoveries.store(0, std::memory_order_relaxed);
}

void count_hle_call(uint32_t nid) {
    if (!g_enabled.load(std::memory_order_relaxed))
        return;

    if (NidSlot *const slot = find_slot(nid))
        slot->calls.fetch_add(1, std::memory_order_relaxed);
    else
        g_overflow_calls.fetch_add(1, std::memory_order_relaxed);
}

void add_hle_time(uint32_t nid, uint64_t nanoseconds) {
    if (!g_time_enabled.load(std::memory_order_relaxed))
        return;

    // The NID was already counted, so its slot exists. A missing one means the
    // table filled between the two calls, which overflow already recorded.
    if (NidSlot *const slot = find_slot(nid))
        slot->nanoseconds.fetch_add(nanoseconds, std::memory_order_relaxed);
    else
        g_overflow_nanoseconds.fetch_add(nanoseconds, std::memory_order_relaxed);
}

void count_translated_instruction() {
    if (!g_enabled.load(std::memory_order_relaxed))
        return;
    g_translated_instructions.fetch_add(1, std::memory_order_relaxed);
}

void count_cache_invalidation(size_t length) {
    if (!g_enabled.load(std::memory_order_relaxed))
        return;
    g_cache_invalidations.fetch_add(1, std::memory_order_relaxed);
    g_cache_invalidation_bytes.fetch_add(length, std::memory_order_relaxed);
}

void count_page_table_read() {
    if (!g_enabled.load(std::memory_order_relaxed))
        return;
    g_page_table_reads.fetch_add(1, std::memory_order_relaxed);
}

void count_page_table_write() {
    if (!g_enabled.load(std::memory_order_relaxed))
        return;
    g_page_table_writes.fetch_add(1, std::memory_order_relaxed);
}

void count_invalid_access_recovery() {
    if (!g_enabled.load(std::memory_order_relaxed))
        return;
    g_invalid_access_recoveries.fetch_add(1, std::memory_order_relaxed);
}

void dump_interval() {
    if (!g_enabled.load(std::memory_order_relaxed))
        return;
    // perf_log owns the files. Writing without it would leave rows nothing
    // ever flushes, so the counters keep accumulating for the next interval.
    if (!perf_log::enabled())
        return;

    const bool with_time = g_time_enabled.load(std::memory_order_relaxed);

    // exchange gives the delta and resets in one step, so a long session reads
    // as a rate instead of a growing total. A count that lands after the
    // exchange belongs to the next interval, which is the intended split.
    // Not const: exchange resets the counters as it reads them.
    for (NidSlot &slot : g_nid_slots) {
        const uint32_t nid = slot.nid.load(std::memory_order_relaxed);
        if (nid == 0)
            continue;

        const uint64_t calls = slot.calls.exchange(0, std::memory_order_relaxed);
        const uint64_t nanoseconds = slot.nanoseconds.exchange(0, std::memory_order_relaxed);
        if (calls == 0 && nanoseconds == 0)
            continue;

        perf_log::write("hle", kHleHeader,
            fmt::format("{:08X},{},{},{}", nid, import_name(nid), calls, with_time ? nanoseconds : 0));
    }

    // A non-zero overflow row is the signal that the table is too small, so it
    // is written even when the interval had no ordinary calls.
    const uint64_t overflow_calls = g_overflow_calls.exchange(0, std::memory_order_relaxed);
    const uint64_t overflow_ns = g_overflow_nanoseconds.exchange(0, std::memory_order_relaxed);
    if (overflow_calls != 0 || overflow_ns != 0) {
        perf_log::write("hle", kHleHeader,
            fmt::format("00000000,<overflow>,{},{}", overflow_calls, overflow_ns));
    }

    struct Row {
        const char *name;
        uint64_t value;
    };
    const Row rows[] = {
        { "translated_instructions", g_translated_instructions.exchange(0, std::memory_order_relaxed) },
        { "cache_invalidations", g_cache_invalidations.exchange(0, std::memory_order_relaxed) },
        { "cache_invalidation_bytes", g_cache_invalidation_bytes.exchange(0, std::memory_order_relaxed) },
        { "page_table_reads", g_page_table_reads.exchange(0, std::memory_order_relaxed) },
        { "page_table_writes", g_page_table_writes.exchange(0, std::memory_order_relaxed) },
        { "invalid_access_recoveries", g_invalid_access_recoveries.exchange(0, std::memory_order_relaxed) },
    };
    for (const Row &row : rows) {
        if (row.value != 0)
            perf_log::write("jit", kJitHeader, fmt::format("{},{}", row.name, row.value));
    }
}

} // namespace hotpath