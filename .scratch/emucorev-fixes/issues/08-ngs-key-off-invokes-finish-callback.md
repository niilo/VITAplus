# 08: sceNgsVoiceKeyOff invokes the finish callback itself

Status: open
Type: task
Label: ready-for-agent

## Context

`sceNgsVoiceKeyOff` (`vita3k/modules/SceNgsUser/SceNgs.cpp:981`) does this:

```cpp
voice->is_keyed_off = true;
system->voice_scheduler.off(emuenv.mem, voice);
voice->is_keyed_off = false;
system->voice_scheduler.stop(emuenv.mem, voice);

// call the finish callback, I got no idea what the module id should be in this case
voice->invoke_callback(emuenv.kernel, emuenv.mem, thread_id, voice->finished_callback, voice->finished_callback_user_data, 0);
return SCE_NGS_OK;
```

Three things are wrong with it.

vitasdk `ngs.h` says `sceNgsVoiceKeyOff` changes the key state and that
processing continues until the voice finishes or is killed. The finish
callback belongs to `sceNgsSystemUpdate`, not to the key-off call. Calling it
here re-enters the guest at a point the game did not expect, on the calling
thread, while the caller may still hold a lock that the callback path takes
again.

The voice is stopped in the same call, so `VoiceScheduler::stop` moves it to
`VOICE_STATE_AVAILABLE` and the envelope release never runs. A sound ends with
a hard cut, which can click. That is the parent ticket,
`.scratch/log-review-2026-09-29/issues/09-ngs-key-off-release.md`.

The callback fires unconditionally, so keying off a voice that is already
stopped emits a second finish callback.

EmuCoreV commit `5745dfc8` moves the callback out of this path. Commit
`c407e1c8` adds the rest of the release handling.

## Plan

1. Remove the `invoke_callback` call from `sceNgsVoiceKeyOff`. Take
   `scheduler.mutex` and the voice's `voice_mutex`
   (`vita3k/ngs/include/ngs/scheduler.h:66`) for the state change, and drop
   both before any callback, so the callback cannot re-enter a held lock.
   `invoke_callback` already returns early on a null callback
   (`vita3k/ngs/src/ngs.cpp:335`).
2. Return early when the voice is not active, so a key-off on a stopped voice
   sends no callback.
3. Let the scheduler run the voice to the end of its release. This is plan
   steps 1 and 2 of the parent ticket: keep the voice in `VOICE_STATE_FINALIZING`
   and in the queue while the envelope releases, then let the scheduler call
   the finish callback and stop the voice.
4. `Atrac9Module::on_state_change` (`vita3k/ngs/src/modules/atrac9.cpp:64`)
   and `PlayerModule::on_state_change`
   (`vita3k/ngs/src/modules/player.cpp:53`) both rewind the read pointer when
   `is_keyed_off` is set. Gate both on the new state being
   `VOICE_STATE_AVAILABLE`, otherwise a voice entering FINALIZING restarts at
   the top of its loop.
5. `EnvelopeModule::process` (`vita3k/ngs/src/modules/envelope.cpp:103`)
   seeds the release from `logical->release_height`, which is initialised to 0
   at `vita3k/ngs/include/ngs/modules/envelope.h:62`. On the first key-off
   tick `releasing` is still false, so the release starts from silence. Seed
   it from the current envelope height instead.
6. `VoiceScheduler::update` (`vita3k/ngs/src/scheduler.cpp:351`) works from a
   snapshot of its queue. A guest callback that runs during the update can
   pause or stop the voice, and the loop then carries on with it. Re-check the
   state and `is_paused` per voice. EmuCoreV also adds a rule for a voice in
   FINALIZING whose only module loops forever: it can never finish itself, so
   it must be finalised rather than left in the queue. That rule references
   `needs_reinit`, which does not exist here
   (`vita3k/ngs/include/ngs/system.h:159` has only `is_bypassed`), so decide
   what replaces it.
7. Test on the device with Uncharted and Killzone Mercenary, per the parent
   ticket. Listen for the click at the end of a note, and check that the voice
   count in the log falls to zero.

## Comments