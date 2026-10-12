# 04: The five _sceKernelCancel* exports are unimplemented

Status: open
Type: task
Label: ready-for-agent

## Context

`vita3k/modules/SceKernelThreadMgr/SceThreadmgr.cpp` implements two of the
cancel family and returns `UNIMPLEMENTED()` from the rest:

| Line | Export | State |
| --- | --- | --- |
| `:50` | `_sceKernelCancelEvent` | `UNIMPLEMENTED()` |
| `:55` | `_sceKernelCancelEventFlag` | calls `eventflag_cancel` |
| `:60` | `_sceKernelCancelEventWithSetPattern` | `UNIMPLEMENTED()` |
| `:65` | `_sceKernelCancelMsgPipe` | `UNIMPLEMENTED()` |
| `:70` | `_sceKernelCancelMutex` | `UNIMPLEMENTED()` |
| `:74` | `_sceKernelCancelRWLock` | `UNIMPLEMENTED()` |
| `:79` | `_sceKernelCancelSema` | calls `semaphore_cancel` |

A game that calls one of the five gets `SCE_KERNEL_ERROR_UNIMPLEMENTED`. It
then has no documented way to recover, and on a real Vita the call succeeds.

`semaphore_cancel` (`vita3k/kernel/src/sync_primitives.cpp:1338`) and
`eventflag_cancel` are the two existing implementations and set the pattern
for the rest. A separate defect sits inside `semaphore_cancel`: the
`setCount > max` check reported at `sync_primitives.cpp:1371` runs after the
waiter loop has already woken and removed the waiters, so a rejected call has
destroyed them. That belongs to this ticket because the cancel paths share the
walk.

EmuCoreV commit `0f40c635` implements all five and fixes the check order.

## Plan

1. For each of the five, find the matching `*_create` in
   `vita3k/kernel/src/sync_primitives.cpp` and the waiter queue the object
   keeps. Wake every waiter, remove it from the queue, and return
   `SCE_KERNEL_OK` with the number of woken threads written to the `pNumWait`
   argument when the NID takes one. Check the NID table under
   `vita3k/modules/SceKernelThreadMgr/` for the real signatures before
   writing them; the file uses a macro, and two of these have an
   `SceUInt32 *` output that must not be dropped.
2. Move the `setCount` validation to the top of `semaphore_cancel`, before
   the waiter loop. Confirm that the `val < setCount` check that remains after
   the loop still runs, because moving the first check does not by itself
   restore a wakeup that was already delivered.
3. Reuse the `on_delete` wake helper added by ticket 03 rather than writing a
   second walk. Do this ticket after 03, or agree the helper's name first.
4. Add a case per export to the kernel tests: create, block a thread on the
   object, cancel, check the waiter count and that no waiter is left in the
   queue.

## Comments