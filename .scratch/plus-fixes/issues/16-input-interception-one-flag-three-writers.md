# 16: One input-interception flag with three writers and two readers

Status: open
Type: task
Label: ready-for-agent

## Context

`vita3k/ctrl/include/ctrl/state.h:78` declares a single flag:

```cpp
std::atomic<bool> overlay_input_intercepted{ false };
```

Three places write it:

- `vita3k/app/src/app_init.cpp:682` stores `true` when the native overlay's
  input loop starts, `:684` stores `false` when it stops.
- `vita3k/app/src/session_controller.cpp:233` stores its own value:
  `overlay_intercepted = paused || input_intercepted`.
- `vita3k/ctrl/include/ctrl/state.h:96` stores `false` on teardown.

Two places read it: `vita3k/ctrl/src/ctrl.cpp:258` and
`vita3k/touch/src/touch.cpp:319`.

The last writer wins, and the two writers do not know about each other. The
session controller runs `apply_runtime_state_locked` for reasons other than a
pause, including a phase change and a config apply. Any of those clears the
overlay's block while the overlay's input loop is still running. The game then
reads the stick and the buttons that the overlay is about to use, so a touch
lands in both the overlay and the game.

`vita3k/app/src/session_controller.cpp:233` also conflates two separate
conditions: being paused, and the overlay asking for the input.

Vita3K-Plus commit `ad93d9c7`.

## Plan

1. Split into two flags, one for the pause state and one for the overlay, and
   have each writer own its own. Add an accessor that reports whether input is
   blocked, so the two readers do not each have to know both flags.
2. Add a count of active input loops, so a stale overlay block with no running
   loop is cleared rather than sticking. `vita3k/overlay/include/overlay/display_manager.h`
   needs the counter.
3. Check `vita3k/ctrl/include/ctrl/state.h:96`, which clears the flag on
   teardown, for the same conflation.
4. Test: start the overlay, trigger a config apply, and check that input stays
   blocked. Then pause and resume with the overlay running.

## Comments