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

#include <kernel/types.h>

struct KernelState;

// Class numbers as the Vita reports them for a UID. A guest uses them to check
// that a handle is the kind it expects before using it, so the values have to
// match hardware rather than this tree's own layout.
enum class UidClass : SceInt32 {
    invalid = 0,
    thread = 1,
    semaphore = 2,
    event_flag = 3,
    mutex = 4,
    cond = 5,
    timer = 6,
    msg_pipe = 7,
    callback = 8,
    thread_event = 9,
    lw_mutex = 10,
    lw_cond = 11,
    rw_lock = 12,
    simple_event = 13,
};

// Looks up a live registered thread-manager object, or returns
// SCE_KERNEL_ERROR_UNKNOWN_UID when nothing owns the uid. export_name is only
// used for the log line, since RET_ERROR logs through it.
SceInt32 get_threadmgr_uid_class(KernelState &kernel, const char *export_name, SceUID uid);