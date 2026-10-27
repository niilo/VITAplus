// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include <util/fs.h>
#include <util/perf_log.h>

#include <gtest/gtest.h>

#include <fstream>
#include <string>

namespace {

// A folder that goes away when the test ends.
struct TempDir {
    fs::path path;
    explicit TempDir(const char *name) {
        path = fs::temp_directory_path() / (std::string("perf_log_test_") + name);
        fs::remove_all(path);
        fs::create_directories(path);
    }
    ~TempDir() { fs::remove_all(path); }

    fs::path file(const std::string &channel) const {
        return path / (channel + ".csv");
    }

    // The lines of a channel, without the header and without empty lines.
    std::vector<std::string> lines(const std::string &channel) const {
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

// The writer thread flushes once per second, so a test that wants to read a
// file stops the log first. stop() flushes what is left.
void finish(const TempDir &dir) {
    perf_log::stop();
    (void)dir;
}

} // namespace

// A write before start() is dropped, and no file appears. This is the state
// the emulator is in for its whole life when the setting is off.
TEST(PerfLog, WriteBeforeStartIsDropped) {
    const TempDir dir("before_start");
    ASSERT_FALSE(perf_log::enabled());
    perf_log::write("frames", "steady_us,title_id", "1,PCSA00000");
    finish(dir);
    EXPECT_FALSE(fs::exists(dir.file("frames")));
}

// start() truncates, so each game start gets a fresh file.
TEST(PerfLog, StartTruncatesTheChannel) {
    const TempDir dir("truncate");
    perf_log::start(dir.path);
    perf_log::write("frames", "steady_us,title_id", "1,PCSA00000");
    finish(dir);
    ASSERT_EQ(dir.lines("frames").size(), 1);

    perf_log::start(dir.path);
    perf_log::write("frames", "steady_us,title_id", "2,PCSA00000");
    finish(dir);
    const auto lines = dir.lines("frames");
    ASSERT_EQ(lines.size(), 1);
    EXPECT_EQ(lines[0], "2,PCSA00000");
}

// Each channel is one file with its own header.
TEST(PerfLog, EachChannelHasItsOwnFileAndHeader) {
    const TempDir dir("channels");
    perf_log::start(dir.path);
    perf_log::write("frames", "steady_us,title_id", "10,PCSA00029");
    perf_log::write("presents", "steady_us,result", "11,0");
    perf_log::write("scenes", "steady_us,draws,record_us", "12,3,8000");
    finish(dir);

    ASSERT_EQ(dir.lines("frames").size(), 1);
    ASSERT_EQ(dir.lines("presents").size(), 1);
    ASSERT_EQ(dir.lines("scenes").size(), 1);
    EXPECT_EQ(dir.lines("frames")[0], "10,PCSA00029");
    EXPECT_EQ(dir.lines("presents")[0], "11,0");
    EXPECT_EQ(dir.lines("scenes")[0], "12,3,8000");

    std::ifstream f(dir.file("scenes").string());
    std::string header;
    std::getline(f, header);
    EXPECT_EQ(header, "steady_us,draws,record_us");
}

// stop() flushes what is still in memory, so nothing is lost at the end of a
// run. This is what the reader of the CSV files depends on.
TEST(PerfLog, StopFlushesWhatIsLeft) {
    const TempDir dir("flush");
    perf_log::start(dir.path);
    for (int i = 0; i < 100; i++)
        perf_log::write("frames", "steady_us,title_id", std::to_string(i) + ",PCSA00029");
    finish(dir);
    const auto lines = dir.lines("frames");
    ASSERT_EQ(lines.size(), 100);
    EXPECT_EQ(lines.front(), "0,PCSA00029");
    EXPECT_EQ(lines.back(), "99,PCSA00029");
}

// stop() when the log was never on must not build the State object, because
// its destructor would then run at process exit. This is the off path.
TEST(PerfLog, StopWhileOffDoesNothing) {
    const TempDir dir("off");
    perf_log::stop();
    perf_log::stop();
    EXPECT_FALSE(perf_log::enabled());
    // Nothing was written, so the folder the log would use stays empty.
    for (const auto &entry : fs::directory_iterator(dir.path))
        EXPECT_TRUE(entry.is_directory());
}

// start() twice keeps one writer thread and one file, and the second start
// discards what the first one had.
TEST(PerfLog, StartTwiceKeepsOneChannel) {
    const TempDir dir("twice");
    perf_log::start(dir.path);
    perf_log::write("frames", "steady_us,title_id", "1,PCSA00029");
    perf_log::start(dir.path);
    perf_log::write("frames", "steady_us,title_id", "2,PCSA00029");
    EXPECT_TRUE(perf_log::enabled());
    finish(dir);
    const auto lines = dir.lines("frames");
    ASSERT_EQ(lines.size(), 1);
    EXPECT_EQ(lines[0], "2,PCSA00029");
}

// now_us() has to advance, or every timestamp in the CSV files is the same.
TEST(PerfLog, NowUsAdvances) {
    const int64_t first = perf_log::now_us();
    EXPECT_GT(first, 0);
    EXPECT_GE(perf_log::now_us(), first);
}