# 09: Keep a voice running after key-off until its envelope release ends

Status: open
Type: task
Label: needs-triage
Blocked by: 03

## Context

vitasdk `ngs.h` says that `sceNgsVoiceKeyOff` changes the key state and
that processing continues until the voice finishes or is killed. The
envelope then plays its release (`uReleaseMsecs`) and the voice ends.

In Vita3K, `sceNgsVoiceKeyOff` (`vita3k/modules/SceNgsUser/SceNgs.cpp`)
calls the finish callback and stops the voice at once. So a sound ends
with a hard cut. This can give a click.

## Plan

1. In `sceNgsVoiceKeyOff`, if the voice has an envelope module that is not
   bypassed and has a release time, keep the voice in FINALIZING and keep
   it in the scheduler queue.
2. The envelope ramps from the current gain to 0 over `uReleaseMsecs`, then
   returns true from `process`. The scheduler then calls the finish
   callback and stops the voice.
3. Check what the player module does while FINALIZING. It must keep
   producing sound.
4. Test with Uncharted and Killzone Mercenary (Vita3K-Plus has notes on
   Killzone voice reuse).

## Comments

### 2026-10-12: EmuCoreV implements this

EmuCoreV (`/home/pielinen/src/EmuCoreV`) is an Android Vita emulator with a
Vita3K core bundled at `app/src/main/cpp/vita3k/`. Two of its commits implement
this ticket.

`5745dfc8` removes the `invoke_callback` call from `sceNgsVoiceKeyOff`. That
covers step 1's first half and is confirmed present here:
`vita3k/modules/SceNgsUser/SceNgs.cpp:981-988` is byte for byte the pre-fix
body, comment included.

`c407e1c8` covers the rest. It covers step 3, which this ticket asks about:
`Atrac9Module::on_state_change` (`vita3k/ngs/src/modules/atrac9.cpp:64`) and
`PlayerModule::on_state_change` (`vita3k/ngs/src/modules/player.cpp:53`) both
rewind the read pointer on `is_keyed_off` with no check on the new state, so a
voice entering FINALIZING restarts at the top of its loop. The same commit
seeds the envelope release from the current height instead of
`logical->release_height`, which is initialised to 0 at
`vita3k/ngs/include/ngs/modules/envelope.h:62`, so the first release of a
voice is silent.

Two things EmuCoreV does that this ticket does not mention, both recorded in
the new ticket:

- `VoiceScheduler::update` (`vita3k/ngs/src/scheduler.cpp:351`) works from a
  snapshot of its queue, so a guest callback that pauses or stops a voice
  during the update does not stop the loop over it.
- A voice in FINALIZING whose only module loops forever can never finish
  itself, so it stays queued and never emits the finish callback.

Neither commit closes this ticket on its own. The work is written up as
`.scratch/emucorev-fixes/issues/08-ngs-key-off-invokes-finish-callback.md`.
