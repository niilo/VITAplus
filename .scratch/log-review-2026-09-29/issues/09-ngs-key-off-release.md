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
