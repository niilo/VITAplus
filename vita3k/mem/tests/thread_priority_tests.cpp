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

#include <gtest/gtest.h>

#include <iostream>

#ifndef _WIN32
#include <cerrno>
#include <sys/resource.h>
#include <sys/time.h>
#endif

namespace {

// The nice value of the calling thread, and whether getpriority succeeded.
// getpriority reports an error as a return of -1, which is also a valid nice
// value, so errno is what decides.
int current_nice(bool *ok) {
#ifdef _WIN32
    (void)ok;
    return 0;
#else
    errno = 0;
    const int n = getpriority(PRIO_PROCESS, 0);
    if (ok)
        *ok = (errno == 0);
    return n;
#endif
}

} // namespace

// A value of 0 means leave it alone, which is the default of the
// thread-nice-renderer setting. The function says so by returning false, and
// the thread keeps whatever nice value it had.
TEST(ThreadPriority, ZeroLeavesTheThreadAlone) {
    bool ok = false;
    const int before = current_nice(&ok);
    ASSERT_TRUE(ok);
    EXPECT_FALSE(util::set_thread_nice(0));
    bool after_ok = false;
    const int after = current_nice(&after_ok);
    ASSERT_TRUE(after_ok);
    EXPECT_EQ(after, before);
}

#ifndef _WIN32
// Raising the priority above normal needs CAP_SYS_NICE or a nonzero
// RLIMIT_NICE. Neither is available to an app process, so the honest contract
// is this: the call reports whether it worked, and the caller logs that.
//
// This test does not assume which way it goes. It checks that the reported
// result matches the value the thread ends up with, which is the part that
// must hold either way.
TEST(ThreadPriority, TheResultMatchesWhatTheThreadEndsUpWith) {
    bool ok = false;
    const int before = current_nice(&ok);
    ASSERT_TRUE(ok);

    const bool applied = util::set_thread_nice(before - 1);
    bool after_ok = false;
    const int after = current_nice(&after_ok);
    ASSERT_TRUE(after_ok);

    if (applied)
        EXPECT_EQ(after, before - 1);
    else
        EXPECT_EQ(after, before);

    // Put the thread back, and let the call report the way it does.
    if (after != before)
        util::set_thread_nice(before);
}

// RLIMIT_NICE is what decides, so record it: a value of 0 means the lowering
// cannot work here, whatever the capability check says.
TEST(ThreadPriority, RecordTheNiceLimit) {
    struct rlimit limit{};
    ASSERT_EQ(getrlimit(RLIMIT_NICE, &limit), 0);
    std::cout << "RLIMIT_NICE soft=" << limit.rlim_cur
              << " hard=" << limit.rlim_max << std::endl;
    if (limit.rlim_cur == 0) {
        // With a zero soft limit the kernel refuses any lowering of the nice
        // value for a process without CAP_SYS_NICE. The test above already
        // covers both outcomes, so nothing is asserted here.
        SUCCEED() << "RLIMIT_NICE is 0, so a lower nice value cannot be set without CAP_SYS_NICE";
    } else {
        SUCCEED() << "RLIMIT_NICE is " << limit.rlim_cur << ", so a lower nice value may be set";
    }
}

// Raising the nice value always works for an unprivileged process, because it
// only lowers the priority. That is the one direction the setting can be
// relied on to do something, and it is the wrong direction for this ticket, so
// the test says so.
TEST(ThreadPriority, RaisingTheNiceValueWorks) {
    bool ok = false;
    const int before = current_nice(&ok);
    ASSERT_TRUE(ok);
    if (before >= 15) {
        GTEST_SKIP() << "the nice value is already near its maximum";
    }
    EXPECT_TRUE(util::set_thread_nice(before + 1));
    bool after_ok = false;
    const int after = current_nice(&after_ok);
    ASSERT_TRUE(after_ok);
    EXPECT_EQ(after, before + 1);
    util::set_thread_nice(before);
}
#endif