# 03: _sceKernelWaitThreadEnd ignores its timeout and takes two locks in the wrong order

Status: open
Type: task
Label: ready-for-agent

## Context

`wait_thread_end` (`vita3k/modules/SceKernelThreadMgr/SceThreadmgr.cpp:855`)
has no timeout parameter:

```cpp
static int wait_thread_end(KernelState &kernel, ThreadStatePtr &waiter, ThreadStatePtr &target, int *stat) {
    std::unique_lock<std::mutex> waiter_lock(waiter->mutex);
    {
        const std::unique_lock<std::mutex> thread_lock(target->mutex);
        if (target->status == ThreadStatus::dormant) {
            ...
```

Both exports at `:882` and `:893` pass four arguments. The `timeout` the guest
supplied is dropped on the floor, and the thread waits on `waiter->status_cond`
until it is signalled. A guest that passes a finite timeout and gets an infinite
wait hangs.

The lock order is also inverted. The function takes `waiter->mutex` at `:856`
and then `target->mutex` at `:858`. Two threads waiting on each other each hold
their own and want the other's, which is a deadlock. The order has to be
`target` then `waiter`, because `target->mutex` is the lock that guards the
state being tested.

The reusable pieces are already here. `handle_timeout`
(`vita3k/kernel/src/sync_primitives.cpp:188`) implements the deadline and the
remaining-time write-back, and `wait_for_run_precise`
(`vita3k/kernel/src/thread.cpp:87`) sleeps on `status_cond` with a microsecond
budget.

What is missing is the cleanup: on a timeout the waiter has to be erased from
`target->waiting_threads`, or a later exit writes through the `stat` pointer the
guest has already reclaimed. `handle_timeout` erases via
`queue->erase(data_it)`, so `ThreadDataQueue` has the iterator-based erase this
needs.

Vita3K-Plus commit `316f1c91`.

## Plan

1. Add the `timeout` parameter to `wait_thread_end` and pass it from both
   exports. Use `wait_for_run_precise` with the remaining budget instead of an
   unbounded wait.
2. Return `SCE_KERNEL_ERROR_WAIT_TIMEOUT` on expiry and write the unused part of
   the timeout back through the `timeout` pointer, matching
   `handle_timeout`'s convention.
3. On the timeout path, erase the waiter's `WaitingThreadData` from
   `target->waiting_threads` and reset its wait reason.
4. Swap the two lock acquisitions to `target->mutex` then `waiter->mutex`.
   Check every caller for a lock it is already holding.
5. Test both: a thread that exits inside the timeout, and one that does not.

## Comments