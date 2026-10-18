# 02: sceKernelCondWait does not check that the caller owns the mutex

Status: open
Type: task
Label: ready-for-agent

## Context

Waiting on a condition variable requires holding its associated mutex. On
hardware, `sceKernelCondWait` with a mutex the caller does not own returns
`SCE_KERNEL_ERROR_MUTEX_NOT_OWNED`, or the `LW_MUTEX` variant, at once.

`condvar_wait` (`vita3k/kernel/src/sync_primitives.cpp:1424`) has no ownership
check. It registers the wait reason at `:1442` and then calls:

```cpp
if (auto error = mutex_unlock_impl(kernel, mem, export_name, thread_id, 1, condvar->associated_mutex))
    return error;
```

That call does not catch a foreign owner. `mutex_unlock_impl` at `:914-923`
logs a foreign unlock, labels it ignored, and falls through to
`return SCE_KERNEL_OK` at `:928`.

So a non-owner registers a wait, unlocks a mutex it does not hold, and parks in
`condvar->waiting_threads`. Nothing can signal it, because the thread that does
own the mutex has no idea a waiter exists. That is an unrecoverable guest hang.
It is also the whole of the Ys VIII boot hang in Vita3K-Plus commit `51f29e2b`.

There is a second consequence. The wait reason is set at `:1442` before the
unlock. On the error path the thread returns with the reason still set.

One fix, not two: `mutex_unlock_impl` returning success on a foreign unlock is
what makes this reachable, and it is also wrong on its own account.

Vita3K-nx added the ownership check, and it reached Vita3K-Plus through commit
`129cc7b2` and then `51f29e2b`.

## Plan

1. At the top of `condvar_wait`, after the condvar is found, check that
   `condvar->associated_mutex` is null or that `owner_id` is `thread_id`.
   Return `SCE_KERNEL_ERROR_MUTEX_NOT_OWNED`, or
   `SCE_KERNEL_ERROR_MUTEX_NOT_OWNED_LW_MUTEX` for a lightweight mutex. The
   naming in `vita3k/kernel/include/kernel/types.h` is the reference.
2. Move the `set_wait_reason` call after the unlock, so an early return cannot
   leave it set.
3. Decide separately whether `mutex_unlock_impl` should return an error for a
   foreign unlock. It is a shared function, so changing it affects every
   primitive. If it is left as it is, say why here; the condvar check is then
   what closes this ticket.
4. Add a test to the kernel suite: create a condvar and its mutex, lock the
   mutex on thread A, call `sceKernelCondWait` on thread B, and check that B
   gets the error and is not in `condvar->waiting_threads`.

## Comments