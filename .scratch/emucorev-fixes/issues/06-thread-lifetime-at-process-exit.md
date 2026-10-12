# 06: Guest host threads are detached and never joined at process exit

Status: open
Type: task
Label: ready-for-agent

## Context

Two defects in the same function, `vita3k/kernel/src/kernel.cpp`.

**The host thread is detached.** At `:191`:

```cpp
params.host_may_destroy_params = SDL_CreateSemaphore(0);
SDL_DetachThread(SDL_CreateThread(&thread_function, thread->name.c_str(), &params));
SDL_WaitSemaphore(params.host_may_destroy_params);
SDL_DestroySemaphore(params.host_may_destroy_params);
```

Nothing joins the SDL thread, and `thread_function` erases the thread from the
registry at `:86-92` before it returns:

```cpp
{
    std::lock_guard<std::mutex> lock(params.kernel->mutex);
    params.kernel->threads.erase(thread->id);
    ...
    params.kernel->thread_deleted_cond.notify_all();
}
```

So `process_exit` can be woken, see an empty map, and return while the host
thread is still unwinding. Thread-local destructors then run afterwards, and
one of them can release a cached `MutexPtr`, whose strong `ThreadStatePtr`
owner frees guest stack and TLS blocks that the host believes are gone.

`ThreadState::execute_guest` uses a thread-local mutex cache
(`vita3k/kernel/src/sync_primitives.cpp:128`, an eight-entry array of
`MutexPtr`). Those are strong references, and there is no kernel identity or
generation in the key, so the cache can also retain guest objects and can
alias a UID reused in a different `KernelState` or after a restart.

**`process_exit` sweeps once.** At `:216`:

```cpp
void KernelState::process_exit() {
    {
        std::lock_guard<std::mutex> lock(mutex);
        for (auto &[_, timer] : timers)
            timer->condvar.notify_all();
        for (auto &[_, thread] : threads)
            thread->exit_delete(false);
    }

    std::unique_lock<std::mutex> lock(mutex);
    thread_deleted_cond.wait(lock, [this] { return threads.empty(); });
}
```

A guest `CreateThread` already in flight publishes its child after the sweep
and before the map is observed empty. That thread then outlives shutdown.

EmuCoreV commit `3958d901` adds a self-contained `HostThreadRegistry`
(76 lines, `kernel/include/kernel/host_threads.h`) and joins every host thread
before `process_exit` returns. Commit `ccec5805` moves the sweep inside the
wait predicate.

## Plan

1. Add `HostThreadRegistry` and hold the SDL thread handle instead of
   detaching. Keep `SDL_CreateThread` under `kernel.mutex`, so a worker that
   finishes quickly cannot exit before its handle is recorded.
2. In `process_exit`, release the lock, then join every host thread. Joining
   while holding `kernel.mutex` deadlocks, because `thread_function` needs it
   to erase.
3. Move the sweep into the wait predicate so it runs again on every wake.
4. Release the local `ThreadStatePtr` in `thread_function` before signalling
   `thread_deleted_cond`, so `process_exit` cannot observe a thread that is
   still registered.
5. Give `KernelState` a generation counter and put it in the mutex cache key
   in `find_mutex`. The cache is per host thread and holds strong references,
   so it must not outlive the kernel it belongs to.
6. Add the new source to `vita3k/kernel/CMakeLists.txt`.
7. Test with a title that exits during a busy frame: Uncharted, then check
   `adb logcat -b crash` with `tools/android/device.sh`.

## Comments