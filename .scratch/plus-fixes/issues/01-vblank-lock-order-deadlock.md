# 01: The vblank thread and a waiting guest thread take the two locks in opposite order

Status: open
Type: task
Label: ready-for-agent

## Context

The vblank thread holds `display.mutex` and then takes a guest thread's mutex.
`vita3k/display/src/display.cpp:157` opens the guard, and `:184` inside it:

```cpp
ThreadStatePtr target_wait = vblank_wait_info.target_thread;

target_wait->update_status(ThreadStatus::run);
```

`wait_vblank` takes them the other way round. `vita3k/display/src/display.cpp:473`:

```cpp
auto thread_lock = std::unique_lock(wait_thread->mutex);

{
    const std::lock_guard<std::mutex> guard(display.mutex);
```

So the guest thread holds thread then display, and the vblank thread holds
display then thread. Two threads that reach this at the same time deadlock, and
because the guest thread is inside a kernel import at that point, the guest
hangs with no diagnostic.

The same function has a second problem at `:491`. It iterates
`display.vblank_callbacks` with no lock, while the vblank thread iterates the
same map under the mutex at `:176`. That is a data race on a `std::map`. It
also calls `cb->execute`, which runs guest code, inside that iteration, so a
callback that registers another vblank callback invalidates the iterator.

VITAplus's other `update_status` calls follow the same pattern as `:184`:
`vita3k/kernel/src/thread.cpp:872` `resume_if_suspended` and `:861` `resume`
both publish `run` without checking the other stop reasons.

EmuCoreV commit `41bada11` and Vita3K-nx both fix this. The Vita3K-nx version
arrived here through Plus commit `129cc7b2`.

## Plan

1. In the vblank thread, collect the waiters whose `target_vcount` has been
   reached into a local vector while holding `display.mutex`, drop the guard,
   then take each `waiter->mutex` and call `update_status` on each. Erase from
   `vblank_wait_infos` under the guard.
2. In `wait_vblank`, take a copy of `display.vblank_callbacks` under the mutex
   before iterating, then run the callbacks outside the iteration. A callback
   that registers another one must not affect the list being walked.
3. Audit every `update_status` call site in `vita3k/kernel/src/` and
   `vita3k/display/src/` for the same lock order. Record what you find here.
4. Test with Uncharted: it presents every frame and uses the display wait.
   `container/vita3k.sh test`, then `container/vita3k.sh format-check`.

## Comments