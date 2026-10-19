// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
// SPDX-License-Identifier: GPL-2.0-or-later

#include <io/state.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <vector>

namespace {
class FileReadTest : public ::testing::Test {
protected:
    void SetUp() override {
        path = fs::temp_directory_path() / fs::unique_path("vita-file-read-%%%%-%%%%-%%%%");
        fs::ofstream file(path, std::ios::binary);
        file.write("abc", 3);
        ASSERT_TRUE(file.good());
    }

    void TearDown() override {
        fs::remove(path);
    }

    fs::path path;
};

TEST_F(FileReadTest, short_read_preserves_every_byte_after_the_file_contents) {
    FileStats file("app0:/test", "test", path, SCE_O_RDONLY);
    std::vector<uint8_t> buffer(32769, 0xa5);
    ASSERT_EQ(file.read(buffer.data(), 1, buffer.size()), 3);
    EXPECT_EQ(buffer[0], 'a');
    EXPECT_EQ(buffer[1], 'b');
    EXPECT_EQ(buffer[2], 'c');
    EXPECT_TRUE(std::all_of(buffer.begin() + 3, buffer.end(), [](uint8_t b) { return b == 0xa5; }));
}

TEST_F(FileReadTest, eof_preserves_the_entire_destination) {
    FileStats file("app0:/test", "test", path, SCE_O_RDONLY);
    std::array<char, 3> contents;
    ASSERT_EQ(file.read(contents.data(), 1, contents.size()), 3);
    std::vector<uint8_t> buffer(32769, 0x5a);
    ASSERT_EQ(file.read(buffer.data(), 1, buffer.size()), 0);
    EXPECT_TRUE(std::all_of(buffer.begin(), buffer.end(), [](uint8_t b) { return b == 0x5a; }));
}
} // namespace
