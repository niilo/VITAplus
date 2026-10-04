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

#include <util/fs.h>
#include <util/hotpath.h>
#include <util/perf_log.h>

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <fstream>
#include <string>
#include <vector>

namespace {

struct TempDir {
    fs::path path;
    explicit TempDir(const char *name) {
        path = fs::temp_directory_path() / (std::string("hotpath_test_") + name);
        fs::remove_all(path);
        fs::create_directories(path);
    }
    ~TempDir() { fs::remove_all(path); }

    fs::path file(const std::string &channel) const {
        return path / (channel + ".csv");
    }

    // perf_log keeps lines in memory and flushes once per second, so a reader
    // has to stop it first. stop() writes out what is left, which is the only
    // flush point a test can rely on.
    std::vector<std::string> lines(const std::string &channel) const {
        perf_log::stop();
        return raw_lines(channel);
    }

    std::vector<std::string> raw_lines(const std::string &channel) const {
        std::vector<std::string> out;
        std::ifstream f(file(channel).string());
        if (!f)
            return out;
        std::string line;
        std::getline(f, line); // the header
        while (std::getline(f, line)) {
            if (!line.empty())
                out.push_back(line);
        }
        return out;
    }
};

// Every test starts from the same state: off, empty, no perf log.
struct HotPathTest : ::testing::Test {
    void SetUp() override {
        perf_log::stop();
        hotpath::set_enabled(false, false);
        hotpath::reset();
    }
    void TearDown() override {
        perf_log::stop();
        hotpath::set_enabled(false, false);
        hotpath::reset();
    }
};

} // namespace

// The default is off, and set_enabled(false) leaves timing off even if the
// caller asks for it, because timing with no counts would add to nothing.
TEST_F(HotPathTest, DisabledByDefault) {
    EXPECT_FALSE(hotpath::enabled());
    EXPECT_FALSE(hotpath::time_enabled());

    hotpath::set_enabled(false, true);
    EXPECT_FALSE(hotpath::enabled());
    EXPECT_FALSE(hotpath::time_enabled());
}

TEST_F(HotPathTest, SetEnabledReportsBothFlags) {
    hotpath::set_enabled(true, false);
    EXPECT_TRUE(hotpath::enabled());
    EXPECT_FALSE(hotpath::time_enabled());

    hotpath::set_enabled(true, true);
    EXPECT_TRUE(hotpath::enabled());
    EXPECT_TRUE(hotpath::time_enabled());
}

// With counting off, nothing reaches a file even when one is open. This is the
// property that lets the counters sit in the dispatch path unconditionally.
TEST_F(HotPathTest, NothingIsWrittenWhileDisabled) {
    const TempDir dir("disabled");
    perf_log::start(dir.path);

    hotpath::count_hle_call(0x12345678);
    hotpath::add_hle_time(0x12345678, 1000);
    hotpath::count_translated_instruction();
    hotpath::count_cache_invalidation(4096);
    hotpath::count_page_table_read();
    hotpath::count_page_table_write();
    hotpath::count_invalid_access_recovery();
    hotpath::dump_interval();

    perf_log::stop();
    EXPECT_EQ(dir.lines("hle").size(), 0u);
    EXPECT_EQ(dir.lines("jit").size(), 0u);
}

// One row per NID, with the name resolved, and the count is what was added.
TEST_F(HotPathTest, CountsAreReportedPerNid) {
    const TempDir dir("counts");
    perf_log::start(dir.path);
    hotpath::set_enabled(true, false);

    hotpath::count_hle_call(0xB295EB61); // sceKernelGetTLSAddr
    hotpath::count_hle_call(0xB295EB61);
    hotpath::count_hle_call(0xB295EB61);
    hotpath::dump_interval();

    const auto rows = dir.lines("hle");
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(rows[0], "B295EB61,sceKernelGetTLSAddr,3,0");
}

// dump_interval resets, so the second interval holds only what arrived after
// the first. This is what makes a long session read as a rate.
TEST_F(HotPathTest, DumpResetsSoIntervalsAreDeltas) {
    const TempDir dir("deltas");
    perf_log::start(dir.path);
    hotpath::set_enabled(true, false);

    hotpath::count_hle_call(0xB295EB61);
    hotpath::count_hle_call(0xB295EB61);
    hotpath::dump_interval();

    hotpath::count_hle_call(0xB295EB61);
    hotpath::dump_interval();

    const auto rows = dir.lines("hle");
    ASSERT_EQ(rows.size(), 2u);
    EXPECT_EQ(rows[0], "B295EB61,sceKernelGetTLSAddr,2,0");
    EXPECT_EQ(rows[1], "B295EB61,sceKernelGetTLSAddr,1,0");
}

// An interval with no activity writes nothing, so a file is not padded with
// zero rows while a game sits on a menu.
TEST_F(HotPathTest, QuietIntervalWritesNothing) {
    const TempDir dir("quiet");
    perf_log::start(dir.path);
    hotpath::set_enabled(true, false);

    hotpath::count_hle_call(0xB295EB61);
    hotpath::dump_interval();
    hotpath::dump_interval();

    EXPECT_EQ(dir.lines("hle").size(), 1u);
}

// The overflow row is what says the table was too small, so it must appear
// rather than a call going missing. NID 0 is the case that reaches it without a
// full table, because 0 marks a free slot.
TEST_F(HotPathTest, NidZeroIsReportedAsOverflow) {
    const TempDir dir("overflow");
    perf_log::start(dir.path);
    hotpath::set_enabled(true, false);

    hotpath::count_hle_call(0);
    hotpath::count_hle_call(0);
    hotpath::dump_interval();

    const auto rows = dir.lines("hle");
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(rows[0], "00000000,<overflow>,2,0");
}

// Two NIDs that hash to the same slot must both be counted. This is the case
// linear probing exists for, so the pair is found rather than left to chance.
TEST_F(HotPathTest, CollidingNidsAreBothCounted) {
    const TempDir dir("collision");
    perf_log::start(dir.path);
    hotpath::set_enabled(true, false);

    // Mirrors slot_for() in hotpath.cpp. When the two agree, this test is
    // exercising the probing path rather than a lucky pair.
    constexpr size_t kTableSize = 1u << 13;
    std::vector<uint32_t> by_slot(kTableSize, 0);
    uint32_t first = 0, second = 0;
    for (uint32_t nid = 1; nid < 400000 && second == 0; nid++) {
        const size_t slot = (static_cast<uint32_t>(nid) * 2654435761u) >> 16 & (kTableSize - 1);
        if (by_slot[slot] != 0) {
            first = by_slot[slot];
            second = nid;
            break;
        }
        by_slot[slot] = nid;
    }
    ASSERT_NE(second, 0u) << "no colliding pair found in the searched range";

    hotpath::count_hle_call(first);
    hotpath::count_hle_call(second);
    hotpath::dump_interval();

    const auto rows = dir.lines("hle");
    ASSERT_EQ(rows.size(), 2u);

    // Whichever order the table resolves them in, both NIDs appear once.
    bool saw_first = false, saw_second = false;
    for (const auto &row : rows) {
        if (row.rfind(fmt::format("{:08X},", first), 0) == 0)
            saw_first = true;
        if (row.rfind(fmt::format("{:08X},", second), 0) == 0)
            saw_second = true;
    }
    EXPECT_TRUE(saw_first);
    EXPECT_TRUE(saw_second);
}

// Self-time is off unless asked for, and it is reported in nanoseconds.
// Both intervals are read at the end, because reading stops the perf log.
TEST_F(HotPathTest, SelfTimeOnlyWhenEnabled) {
    const TempDir dir("time");
    perf_log::start(dir.path);

    hotpath::set_enabled(true, false);
    hotpath::count_hle_call(0xB295EB61);
    hotpath::add_hle_time(0xB295EB61, 500);
    hotpath::dump_interval();

    hotpath::set_enabled(true, true);
    hotpath::count_hle_call(0xB295EB61);
    hotpath::add_hle_time(0xB295EB61, 500);
    hotpath::add_hle_time(0xB295EB61, 250);
    hotpath::dump_interval();

    const auto rows = dir.lines("hle");
    ASSERT_EQ(rows.size(), 2u);
    // Timing off: the duration is not recorded, so the column reads 0.
    EXPECT_EQ(rows[0], "B295EB61,sceKernelGetTLSAddr,1,0");
    EXPECT_EQ(rows[1], "B295EB61,sceKernelGetTLSAddr,1,750");
}

// The translator counters each get their own row, and a zero one is left out
// rather than written as 0.
TEST_F(HotPathTest, TranslatorCountersGetTheirOwnRows) {
    const TempDir dir("translator");
    perf_log::start(dir.path);
    hotpath::set_enabled(true, false);

    hotpath::count_translated_instruction();
    hotpath::count_translated_instruction();
    hotpath::count_translated_instruction();
    hotpath::count_cache_invalidation(4096);
    hotpath::count_page_table_read();
    hotpath::count_page_table_write();
    hotpath::count_page_table_write();
    hotpath::count_page_table_write();
    hotpath::count_invalid_access_recovery();
    hotpath::dump_interval();

    const auto rows = dir.lines("jit");
    ASSERT_EQ(rows.size(), 6u);
    EXPECT_NE(std::find(rows.begin(), rows.end(), "translated_instructions,3"), rows.end());
    EXPECT_NE(std::find(rows.begin(), rows.end(), "cache_invalidations,1"), rows.end());
    EXPECT_NE(std::find(rows.begin(), rows.end(), "cache_invalidation_bytes,4096"), rows.end());
    EXPECT_NE(std::find(rows.begin(), rows.end(), "page_table_reads,1"), rows.end());
    EXPECT_NE(std::find(rows.begin(), rows.end(), "page_table_writes,3"), rows.end());
    EXPECT_NE(std::find(rows.begin(), rows.end(), "invalid_access_recoveries,1"), rows.end());
}

// reset() clears the counts, so a second game in the same process does not
// inherit the first one's totals.
TEST_F(HotPathTest, ResetClearsCounts) {
    const TempDir dir("reset");
    perf_log::start(dir.path);
    hotpath::set_enabled(true, false);

    hotpath::count_hle_call(0xB295EB61);
    hotpath::count_hle_call(0xB295EB61);
    hotpath::reset();
    hotpath::count_hle_call(0xB295EB61);
    hotpath::dump_interval();

    const auto rows = dir.lines("hle");
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(rows[0], "B295EB61,sceKernelGetTLSAddr,1,0");
}

// Without the perf log there is no file to write to, so a dump writes nothing.
TEST_F(HotPathTest, NoPerfLogMeansNoRows) {
    const TempDir dir("nolog");
    hotpath::set_enabled(true, false);

    hotpath::count_hle_call(0xB295EB61);
    hotpath::dump_interval();

    EXPECT_EQ(dir.lines("hle").size(), 0u);
}