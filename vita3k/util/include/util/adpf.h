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

#include <cstdint>

// Android's Performance Hint Manager, ADPF. One session names the threads a
// frame depends on and the period they are expected to finish in, so the system
// can place them and schedule them.
//
// Only the Android build uses this. Everywhere else every entry point is a
// no-op, so callers do not need an #ifdef.
namespace adpf {

// Start a session over `tids` with `target_work_duration_ns` as the period each
// of them is expected to finish in. Returns false when the setting is off, when
// the platform has no hint manager, or when the session could not be created.
//
// Calling this again with a different thread set closes the old session and
// makes a new one, because Session.setThreads is API 34 and this build targets
// API 33. Every recreate is logged with `reason`.
bool start(const int *tids, int tid_count, int64_t target_work_duration_ns, const char *reason);

// Close the session, if there is one.
void stop();

// Report the measured wall time of one frame's work. The system rate-limits its
// own callers, so call this once per frame rather than more often.
void report_work(int64_t actual_work_duration_ns);

// The period the device says it prefers, which is not necessarily one frame.
// Zero when the platform does not answer.
int64_t preferred_update_rate_nanos();

} // namespace adpf