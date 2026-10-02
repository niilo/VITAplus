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

#include <cstring>

#ifndef _WIN32
#include <cerrno>
#include <sys/resource.h>
#include <sys/time.h>
#endif

#ifdef __linux__
#include <sys/prctl.h>
#elif defined(__APPLE__)
#include <pthread.h>
#endif

namespace util {

#ifdef __linux__
// Linux stores the name in `comm`, which is TASK_COMM_LEN bytes including the
// terminating zero. prctl does not report a cut, so a name that does not fit is
// refused here instead of becoming a prefix that matches another thread.
static constexpr size_t comm_len = 16;
#endif

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

bool set_thread_name(const char *name) {
    if (name == nullptr || name[0] == '\0')
        return false;

#ifdef _WIN32
    // The Windows equivalent is SetThreadDescription, which needs Windows 10
    // version 1607. Nothing in this repository measures Windows, so no call is
    // made and no failure is logged.
    (void)name;
    return false;
#elif defined(__linux__)
    if (strlen(name) >= comm_len) {
        LOG_INFO("Thread name '{}' is longer than the {} bytes Linux keeps, so it was not set", name, comm_len - 1);
        return false;
    }
    if (prctl(PR_SET_NAME, name, 0, 0, 0) != 0) {
        LOG_INFO("Could not set the name of this thread to '{}': {}", name, strerror(errno));
        return false;
    }
    return true;
#elif defined(__APPLE__)
    // pthread_setname_np returns the error number itself and does not set
    // errno, so the returned value is the one to report, not errno.
    const int err = pthread_setname_np(name);
    if (err != 0) {
        LOG_INFO("Could not set the name of this thread to '{}': {}", name, strerror(err));
        return false;
    }
    return true;
#else
    (void)name;
    return false;
#endif
}

} // namespace util