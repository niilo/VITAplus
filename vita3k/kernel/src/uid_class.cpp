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

#include <kernel/state.h>
#include <kernel/uid_class.h>

#include <util/log.h>

SceInt32 get_threadmgr_uid_class(KernelState &kernel, const char *export_name, const SceUID uid) {
    if (uid < 0) {
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_UID);
    }

    // Every map is guarded by kernel.mutex, and all of them are read under one
    // acquisition here, so no UID can appear in one map between two lookups.
    const std::lock_guard<std::mutex> lock(kernel.mutex);

    if (kernel.threads.contains(uid))
        return static_cast<SceInt32>(UidClass::thread);
    if (kernel.semaphores.contains(uid))
        return static_cast<SceInt32>(UidClass::semaphore);
    if (kernel.eventflags.contains(uid))
        return static_cast<SceInt32>(UidClass::event_flag);
    if (kernel.mutexes.contains(uid))
        return static_cast<SceInt32>(UidClass::mutex);
    if (kernel.condvars.contains(uid))
        return static_cast<SceInt32>(UidClass::cond);
    if (kernel.timers.contains(uid))
        return static_cast<SceInt32>(UidClass::timer);
    if (kernel.msgpipes.contains(uid))
        return static_cast<SceInt32>(UidClass::msg_pipe);
    if (kernel.callbacks.contains(uid))
        return static_cast<SceInt32>(UidClass::callback);
    if (kernel.lwmutexes.contains(uid))
        return static_cast<SceInt32>(UidClass::lw_mutex);
    if (kernel.lwcondvars.contains(uid))
        return static_cast<SceInt32>(UidClass::lw_cond);
    if (kernel.rwlocks.contains(uid))
        return static_cast<SceInt32>(UidClass::rw_lock);
    if (kernel.simple_events.contains(uid))
        return static_cast<SceInt32>(UidClass::simple_event);

    return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_UID);
}