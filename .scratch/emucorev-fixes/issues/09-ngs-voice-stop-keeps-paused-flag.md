# 09: VoiceScheduler::stop leaves is_paused set

Status: open
Type: task
Label: ready-for-agent

## Context

`vita3k/ngs/src/scheduler.cpp:124`:

```cpp
bool VoiceScheduler::stop(const MemState &mem, Voice *voice) {
    if (voice->state != VOICE_STATE_ACTIVE && voice->state != VOICE_STATE_FINALIZING)
        return false;

    voice->transition(mem, VOICE_STATE_AVAILABLE);
    if (!voice->is_paused)
        deque_voice(Ptr<Voice>(voice, mem));

    return true;
}
```

`is_paused` is never cleared. It is set by `sceNgsVoicePause` and checked
here to decide whether to put the voice back in the update queue.

So a voice that is paused and then stopped stays flagged as paused. A later
`sceNgsVoicePlay` reaches the same check, sees `is_paused` still true, and
re-inserts nothing. The voice never returns to the queue and never produces
sound again, for the rest of the session.

`available` is the state a stopped voice goes back to, so the flag belongs
there with the other voice state that `transition` resets.

EmuCoreV commit `5e4f7c58` change 14 adds one assignment.

## Plan

1. Clear `voice->is_paused` in `stop` before the `deque_voice` check, after
   `transition` to `VOICE_STATE_AVAILABLE`.
2. Audit the other state transitions in `vita3k/ngs/src/scheduler.cpp` for
   the same pattern: any flag that gates `deque_voice` must be reset on the
   path back to `VOICE_STATE_AVAILABLE`, not only on the path to
   `VOICE_STATE_ACTIVE`. Record what you find as a comment here.
3. Add a test to the NGS suite: pause a voice, stop it, play it again, and
   check that the voice is in the scheduler queue and in `VOICE_STATE_ACTIVE`.
   `vita3k/ngs/tests/` already holds the DSP tests, so this fits there.
4. Do this before ticket 08. That ticket changes the key-off path, which calls
   `stop` on every key-off.

## Comments