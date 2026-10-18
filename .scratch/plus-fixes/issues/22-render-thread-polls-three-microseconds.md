# 22: The render thread polls for three microseconds and the comment says three milliseconds

Status: open
Type: task
Label: ready-for-agent

## Context

`vita3k/renderer/src/batch.cpp:198-199`:

```cpp
// Try to wait for a batch (about 2 or 3ms, game should be fast for this)
auto cmd_list = state.command_buffer_queue.top(3);
```

`Queue::top` takes microseconds. `vita3k/threads/include/threads/queue.h:43`:

```cpp
condempty_.wait_for(mlock, std::chrono::microseconds(ms));
```

So `top(3)` waits three microseconds. The comment is wrong by a factor of a
thousand.

`top` returns immediately when the queue is not empty (`:38` and `:47`), so
only the idle case is affected. On this path that means the render thread wakes,
finds nothing, and goes back to sleep three microseconds later. It burns a core
and adds latency to every frame boundary.

The second half of the reference change is to wake the queue instead of polling.
Four sites set `should_display = true` and none of them notifies the command
buffer queue: `vita3k/display/src/display.cpp:168`, `:575`, `:593` and
`vita3k/app/src/app.cpp:370`. With a wake, the wait can be a real one.

Vita3K-Plus commit `7451f9f4`, from Vita3K-nx.

## Plan

1. Decide the intended idle wait. `top(3000)` matches the comment, but a wake is
   better: raise the timeout so a missed wake still recovers, and add a `wake()`
   to `Queue` that the four sites call after setting `should_display`.
2. Check whether this repository's hot-path counters on the render thread are
   affected, and record the before and after numbers. `.scratch/cpu-hotpath-counters/`
   has the counters.
3. Measure with `tools/android/gameplay_scene.sh` and
   `tools/android/device_power_sample.sh`. A busy-wait that yields shows up as
   device power, so the power measurement is the one that matters here.
4. Quote the `driverID` line with any result, per the rule in `CLAUDE.md`.

## Comments