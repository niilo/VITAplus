# 06: _sceKernelGetThreadInfo leaves status unset and can leave name unterminated

Status: open
Type: task
Label: ready-for-agent

## Context

`vita3k/modules/SceKernelThreadMgr/SceThreadmgr.cpp:516-524` writes seven
fields and leaves the rest of the struct as it was on the guest's stack:

```cpp
strncpy(info->name, thread->name.c_str(), KERNELOBJECT_MAX_NAME_LENGTH);
info->stack = Ptr<void>(thread->stack.get());
info->stackSize = thread->stack_size;
info->initPriority = thread->priority; // Todo Give only current priority
info->currentPriority = thread->priority;
info->initCpuAffinityMask = thread->affinity_mask; // Todo Give init affinity
info->currentCpuAffinityMask = thread->affinity_mask;
info->entry = SceKernelThreadEntry(thread->entry_point);
```

`SceKernelThreadInfo` is at `vita3k/kernel/include/kernel/types.h:813`. Not
written: `processId`, `attr`, `status`, `currentCpuId`, `lastExecutedCpuId`,
`initialExitStatus`.

`status` is the one that matters. A guest that branches on it reads whatever
was on its stack. A title that validates a thread handle through this field
crashes. Dragon Quest Heroes II does.

`name` is also wrong at the boundary. `info->name` is
`char[KERNELOBJECT_MAX_NAME_LENGTH + 1]` (`types.h:819`), so a name of exactly
`KERNELOBJECT_MAX_NAME_LENGTH` characters leaves `name[31]` holding guest data
rather than a terminator.

`thread->status` and `thread->returned_value` are read without holding
`thread->mutex`.

Vita3K-Plus commits `601374c6` and `ca3ad408`.

## Plan

1. `memset(info, 0, sizeof(*info))` before the fills, then restore the two
   fields a guest sets: `info->size = sizeof(*info)` and `info->processId = 1`.
2. Terminate the name explicitly:
   `info->name[KERNELOBJECT_MAX_NAME_LENGTH] = '\0';`
3. Add an `SceKernelThreadStatus` enum in
   `vita3k/kernel/include/kernel/types.h` and fill `info->status` from a
   switch on `thread->status`. Check `vita3k/kernel/src/thread.cpp` for the
   full set of values the `ThreadStatus` enum can take.
4. Derive `currentCpuId` and `lastExecutedCpuId` from `affinity_mask`.
5. Take `thread->mutex` around the reads of `status` and `returned_value`.
   Check the caller does not already hold it.
6. Test: fill a `SceKernelThreadInfo` with a poison pattern, call the export,
   and check every field.

## Comments