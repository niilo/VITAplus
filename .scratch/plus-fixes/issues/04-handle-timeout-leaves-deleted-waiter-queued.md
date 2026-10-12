# 04: A thread marked for deletion stays queued in the mutex it was waiting on

Status: open
Type: task
Label: ready-for-agent

## Context

`mutex_lock_impl` (`vita3k/kernel/src/sync_primitives.cpp:817`) loops on
`handle_timeout` and breaks on three conditions:

```cpp
if (res != SCE_KERNEL_OK || mutex->owner_id == thread_id || thread->is_delete_requested())
    break;
```

The break does not remove the `WaitingThreadData`. It was pushed at `:815`
and it stays in `mutex->waiting_threads`.

Nothing sweeps those queues when a thread is deleted. `raise_waiting_threads`
(`vita3k/kernel/src/thread.cpp:184`) is the reverse direction. So the entry
survives, holding a strong `ThreadStatePtr` to a thread that is being torn
down, for the life of the mutex. The next `sceKernelUnlockMutex` wakes it.

That is the pattern in ticket 03 of `.scratch/emucorev-fixes/` from the other
side: there, a sync object with waiters is not deleted at all. Here, a deleted
thread is not removed from the object. Both leave a stale reference in a queue.

The fix belongs in `handle_timeout`, which every waiter already funnels
through, rather than in each of the six call sites. `handle_timeout` already
has an `unwind()` that erases the waiter from the queue.

Vita3K-Plus commit `363285b8`, which came from Vita3K-nx.

## Plan

1. Move the delete-requested check into `handle_timeout` and erase the waiter
   from `queue` through the existing `unwind()` path, so the no-timeout branch
   is covered as well.
2. Remove the now-redundant branch from `mutex_lock_impl`.
3. Audit the other five primitives that call `handle_timeout`. Each has the
   same shape. Report anything that does not as a comment here.
4. This interacts with ticket 03 of `.scratch/emucorev-fixes/`, which rewrites
   the delete functions and moves the waiter bookkeeping. Do that one first, or
   agree the shape of the queue removal before starting.
5. Test: block a thread on a mutex, delete the thread, unlock the mutex, and
   check that nothing is woken and the queue is empty.

## Comments