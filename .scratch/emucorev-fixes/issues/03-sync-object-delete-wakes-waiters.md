# 03: Deleting a sync object must wake the threads waiting on it

Status: open
Type: task
Label: ready-for-agent

## Context

All six `*_delete` functions in `vita3k/kernel/src/sync_primitives.cpp` take
the same shape. At `:961`, for a mutex:

```cpp
if (mutex->waiting_threads->empty()) {
    mutex->deleted.store(true, std::memory_order_relaxed);
    const std::lock_guard<std::mutex> kernel_guard(kernel.mutex);
    mutexes->erase(mutexid);
} else {
    // TODO:
    LOG_WARN("Can't delete sync object, it has waiting threads.");
}
```

The same `LOG_WARN` is at `:440`, `:967`, `:1163`, `:1332`, `:1568` and
`:2080`: simple event, mutex, rwlock, semaphore, condvar and eventflag.

So an object with waiters is not deleted, and the function still returns
`SCE_KERNEL_OK`. A guest thread blocked on an object another thread deleted
then waits forever. The guest has no way to tell that the delete did not
happen, because the return value says it did.

On the Vita, deleting a sync object wakes its waiters with
`SCE_KERNEL_ERROR_WAIT_DELETE`. A lightweight mutex returns
`SCE_KERNEL_ERROR_WAIT_DELETE_LW_MUTEX` and a lightweight condvar returns
`SCE_KERNEL_ERROR_WAIT_DELETE_LW_COND`.

`sync_primitives.h:83` holds only `std::mutex mutex;` on `SyncPrimitive`.
There is no `deleted` flag, no helper that rechecks liveness under the object
lock, and no wake-on-delete path.

EmuCoreV commit `0f40c635` adds a `deleted` flag, a `lock()` that rechecks
liveness while holding the object lock, an `on_delete()` wake-all, and a
`remove_sync_object()` that pulls the object out of the registry under
`kernel.mutex` and then marks and wakes it under the object lock.

## Plan

1. Add `std::atomic<bool> deleted` to `SyncPrimitive` in
   `vita3k/kernel/include/kernel/sync_primitives.h`. A waiter that takes the
   object lock must re-check it, because a delete already under way will not
   see the waiter.
2. Add a `remove_sync_object` helper: erase from the registry under
   `kernel.mutex`, then set `deleted` and wake every waiter under the object
   lock. Use it in all six delete functions.
3. Give each waiter the right error code. `Condvar::lightweight` is needed to
   choose between `SCE_KERNEL_ERROR_WAIT_DELETE_COND` and
   `SCE_KERNEL_ERROR_WAIT_DELETE_LW_COND`.
4. The waiter queues here are `WaitingThreadQueuePtr` over
   `ThreadDataQueue<WaitingThreadData>`, not the queue type EmuCoreV uses, so
   the wake step is a walk that sets `was_canceled` and moves each waiter to
   `run`. `semaphore_cancel` at `sync_primitives.cpp:1338` already does this
   and is the pattern to copy.
5. Do not add a `caller` parameter to `mutex_lock_impl`. The condition
   variable path here already holds the `ThreadStatePtr`.
6. Tests in the existing `vita3k/kernel/tests/` target: delete an object with
   a waiter, check the waiter's status and its recorded error, and check that
   a later create with the same name succeeds.

## Comments