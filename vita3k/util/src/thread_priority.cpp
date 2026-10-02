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

#include <util/thread_priority.h>

#include <util/log.h>

#ifndef _WIN32
#include <cerrno>
#include <sys/resource.h>
#include <sys/time.h>
#endif

namespace util {

bool set_thread_nice(int nice) {
    if (nice == 0)
        return false;

#ifdef _WIN32
    // Windows has no per-thread nice value. The equivalent is a thread
    // priority, which SetThreadPriority takes for the calling thread only.
    // Nothing in this repository measures Windows, so leave it alone.
    (void)nice;
    return false;
#else
    // PRIO_PROCESS with who 0 is the calling thread on Linux.
    //
    // The kernel allows a lower nice value only with CAP_SYS_NICE or a
    // nonzero RLIMIT_NICE soft limit. An app process has neither, so this
    // usually fails. A higher value always works, because that only lowers the
    // priority.
    if (setpriority(PRIO_PROCESS, 0, nice) != 0) {
        LOG_INFO("Could not set the nice value of this thread to {}: {}", nice, strerror(errno));
        return false;
    }
    return true;
#endif
}

} // namespace util