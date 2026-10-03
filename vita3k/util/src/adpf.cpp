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

#include <util/adpf.h>

#include <util/log.h>

#include <algorithm>
#include <atomic>
#include <mutex>
#include <vector>

// The Performance Hint Manager arrived in API 33 and this is the only Android
// API the emulator uses.
//
// <android/performance_hint.h> cannot be included here. The NDK marks every one
// of these calls unavailable because the build targets minSdk, which is 28, and
// raising the API level for the file is not usable: __ANDROID_API__ also selects
// the libc++ feature set, and forcing it breaks std::condition_variable. Neither
// -D__ANDROID_API__=33 nor a diagnostic pragma lifts it.
//
// So the five entry points are declared here as weak symbols with the
// signatures from the header. Weak means a device without the library resolves
// them to null, and every call below is guarded, so the app is correct on a
// device older than Android 13.
#ifdef __ANDROID__
struct APerformanceHintManager;
struct APerformanceHintSession;

extern "C" {
APerformanceHintManager *APerformanceHint_getManager() __attribute__((weak));
APerformanceHintSession *APerformanceHint_createSession(APerformanceHintManager *manager, const int32_t *thread_ids, size_t size, int64_t initial_target_work_duration_nanos) __attribute__((weak));
int APerformanceHint_updateTargetWorkDuration(APerformanceHintSession *session, int64_t target_duration_nanos) __attribute__((weak));
int APerformanceHint_reportActualWorkDuration(APerformanceHintSession *session, int64_t actual_duration_nanos) __attribute__((weak));
int64_t APerformanceHint_getPreferredUpdateRateNanos(APerformanceHintManager *manager) __attribute__((weak));
void APerformanceHint_closeSession(APerformanceHintSession *session) __attribute__((weak));
}
#endif

namespace adpf {

namespace {

std::atomic_bool s_wanted = false;
std::atomic<int64_t> s_target_ns = 0;

#ifdef __ANDROID__
std::mutex s_mutex;
APerformanceHintSession *s_session = nullptr;
std::vector<int> s_tids;

void close_session_locked() {
    if (!s_session)
        return;
    if (APerformanceHint_closeSession)
        APerformanceHint_closeSession(s_session);
    s_session = nullptr;
}

void close_session() {
    std::lock_guard lock(s_mutex);
    close_session_locked();
}
#endif

} // namespace

bool start(const int *tids, int tid_count, int64_t target_work_duration_ns, const char *reason) {
    if (tid_count <= 0 || !tids)
        return false;

    s_target_ns.store(target_work_duration_ns, std::memory_order_relaxed);

#ifdef __ANDROID__
    std::lock_guard lock(s_mutex);

    // The thread set is part of the session, so a change needs a new session.
    // Session.setThreads is API 34 and this builds against API 33.
    std::vector<int> wanted(tids, tids + tid_count);
    std::sort(wanted.begin(), wanted.end());
    wanted.erase(std::unique(wanted.begin(), wanted.end()), wanted.end());

    if (s_session && wanted == s_tids) {
        // Same threads: only the period can have changed.
        if (target_work_duration_ns > 0 && APerformanceHint_updateTargetWorkDuration)
            APerformanceHint_updateTargetWorkDuration(s_session, target_work_duration_ns);
        return true;
    }

    // Every one of these is a weak symbol, so a device without the library
    // resolves it to null and the hint is simply not available.
    APerformanceHintManager *manager = APerformanceHint_getManager ? APerformanceHint_getManager() : nullptr;
    if (!manager || !APerformanceHint_createSession) {
        LOG_WARN_ONCE("[ADPF] this device has no Performance Hint Manager, so the hint is inert");
        return false;
    }

    close_session_locked();

    s_session = APerformanceHint_createSession(manager, wanted.data(), wanted.size(), target_work_duration_ns);
    if (!s_session) {
        LOG_WARN("[ADPF] could not create a session (reason: {})", reason);
        return false;
    }

    s_tids = wanted;
    s_wanted.store(true, std::memory_order_relaxed);

    LOG_INFO("[ADPF] session created for {} thread(s) at a target of {} ns (reason: {})", wanted.size(), target_work_duration_ns, reason);
    const int64_t preferred = APerformanceHint_getPreferredUpdateRateNanos ? APerformanceHint_getPreferredUpdateRateNanos(manager) : 0;
    if (preferred > 0)
        LOG_INFO("[ADPF] the device prefers a period of {} ns", preferred);
    else
        LOG_INFO("[ADPF] the device reports no preferred period");
    return true;
#else
    (void)reason;
    return false;
#endif
}

void stop() {
    s_wanted.store(false, std::memory_order_relaxed);
#ifdef __ANDROID__
    close_session();
#endif
}

void report_work(int64_t actual_work_duration_ns) {
    if (!s_wanted.load(std::memory_order_relaxed) || actual_work_duration_ns <= 0)
        return;
#ifdef __ANDROID__
    std::lock_guard lock(s_mutex);
    if (s_session && APerformanceHint_reportActualWorkDuration)
        APerformanceHint_reportActualWorkDuration(s_session, actual_work_duration_ns);
#endif
}

int64_t preferred_update_rate_nanos() {
#ifdef __ANDROID__
    APerformanceHintManager *manager = (APerformanceHint_getManager && APerformanceHint_getPreferredUpdateRateNanos) ? APerformanceHint_getManager() : nullptr;
    if (!manager)
        return 0;
    return APerformanceHint_getPreferredUpdateRateNanos(manager);
#else
    return 0;
#endif
}

} // namespace adpf