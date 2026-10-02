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

#pragma once

// Host thread priority. This is the only thread control an Android app has:
//
//   setpriority on the calling thread   allowed
//   setpriority on another thread       EPERM, needs CAP_SYS_NICE
//   pthread_setschedparam SCHED_FIFO    EPERM, needs CAP_SYS_NICE
//   sched_setaffinity narrower than the current mask   allowed
//
// Android also asks an app not to set CPU affinity, because the device often
// ignores it and the system places the thread on a core of the right type
// better than the app can. So there is no affinity call here.
//
// Whether lowering the nice value actually works is not decided here. The
// kernel allows it only with CAP_SYS_NICE or a nonzero RLIMIT_NICE, and an app
// process has neither by default. It returns what happened so the caller can
// log it and a measurement can settle it. See
// .scratch/pocket-s-android13/issues/11-thread-priority.md.
namespace util {

// Set the nice value of the calling thread. `nice` is the target value, for
// example -10. A value of 0 means leave it alone.
//
// Call this from inside the thread it applies to: a change for another thread
// needs CAP_SYS_NICE.
//
// Returns true when the nice value is now `nice`, and false when the call
// failed or when `nice` was 0.
bool set_thread_nice(int nice);

} // namespace util