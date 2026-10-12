# 05: simple_event_delete erases from the wrong map

Status: open
Type: task
Label: ready-for-agent

## Context

`simple_event_delete` (`vita3k/kernel/src/sync_primitives.cpp:424`) looks the
object up in one map and erases it from another:

```cpp
const SimpleEventPtr event = lock_and_find(event_id, kernel.simple_events, kernel.mutex);
...
if (event->waiting_threads->empty()) {
    const std::lock_guard<std::mutex> kernel_lock(kernel.mutex);
    kernel.eventflags.erase(event_id);
}
```

`kernel.eventflags.erase` does nothing, because the UID was never in
`eventflags`. The simple event stays in `kernel.simple_events` with its UID
still findable.

What follows from that:

- `sceKernelDeleteEvent` reports success and the object is still live, so a
  later create with the same name collides.
- A later `sceKernelOpenSimpleEvent` on the same name returns the old UID and
  hands back an object the guest believes it deleted.
- The UID is never returned to the pool, so the registry grows over a long
  session.

The six other `*_delete` functions erase from the map they read, so this is
one function with the wrong name in it. EmuCoreV commit `82de10e5` change 4
replaces it with `kernel.simple_events.erase(event_id)`.

Ticket 03 rewrites these delete functions. If 03 lands first this line moves
with it, so do this one first, or fold it into 03.

## Plan

1. Change `kernel.eventflags.erase(event_id)` to
   `kernel.simple_events.erase(event_id)` at
   `vita3k/kernel/src/sync_primitives.cpp:437`.
2. Audit the other delete functions for the same mistake by checking that the
   erase target and the lookup target are the same member in each one. Report
   anything else found as a comment here.
3. Add a test: create a simple event, delete it, create another with the same
   name, and check that the second create returns a different UID. The test
   belongs in the existing kernel test target.

## Comments

### 2026-10-12: Vita3K-Plus has the same one-word change

Vita3K-Plus commit `987bfea4` carries it, together with the missing
`sceKernelOpenSimpleEvent`, which is
`.scratch/plus-fixes/issues/05-sce-kernel-open-simple-event-unimplemented.md`.
Take both from that one commit.