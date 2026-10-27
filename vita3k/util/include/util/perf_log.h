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
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#pragma once

#include <util/fs.h>

#include <cstdint>
#include <string>
#include <string_view>

// Performance log for measurements on a device (setting "perf-log").
// Each channel is one CSV file in the perf folder. Lines are kept in memory and
// a background thread writes them once per second. The thread holds the lock
// while it writes, so a caller can wait for the disk while a flush runs.
namespace perf_log {

// Truncate the CSV files in `dir` and start to accept lines.
void start(const fs::path &dir);
// Write the lines that are left and stop to accept lines.
void stop();
bool enabled();

// Microseconds of std::chrono::steady_clock.
int64_t now_us();

// Add one line to the file `<channel>.csv`. The first call for a channel
// writes `header` as the first line. `line` has no line end.
void write(std::string_view channel, std::string_view header, std::string line);

} // namespace perf_log
