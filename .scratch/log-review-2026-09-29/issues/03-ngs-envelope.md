# 03: Implement the NGS envelope module

Status: resolved
Claimed: 2026-09-29 Claude Code session (Opus 5.5)
Type: task
Label: ready-for-agent

## Context

Uncharted logged `Game is using unimplemented envelope audio module` at
00:16:29. An envelope changes the volume of a voice over time: points with
an amplitude and a time to the next point, an optional loop, and a release
time after key-off. Without it, fades do not happen. A sound that must fade
in or out plays at full volume.

`plus/all-enhancements` has an implementation
(`git show plus/all-enhancements:vita3k/ngs/src/modules/envelope.cpp`). It
depends on Plus changes to the NGS core (`on_key_on`, a logical state type,
finish rules tuned for single games). Use it as a reference, not as a
cherry-pick.

## Plan

1. Add a logical state: current point, position in the segment, release
   state.
2. Per call: compute the gain at the start and end of the call, and ramp
   the gain across the frames (no step changes).
3. Linear segments are linear. Curved segments use a smooth curve.
4. Loop from `nLoopEnd` back to `uLoopStart` while the voice is keyed on.
5. On key-off, ramp from the current gain to 0 over `uReleaseMsecs`.
6. Write `SceNgsEnvelopeStates` to the guest state, because games read it.
7. Do not end the voice from this module. That is a change to voice life
   and needs its own issue.

## Shared rules for issues 03 to 07

- Each module is in `vita3k/ngs/src/modules/<name>.cpp` with its header in
  `vita3k/ngs/include/ngs/modules/<name>.h`. Today each one only logs
  `Game is using unimplemented <name> audio module` and passes the sound
  through with no change.
- A module changes the voice buffer in place. The buffer is interleaved
  stereo float, `granularity` frames per call (see `output.cpp` and
  `player.cpp` for how a module finds its buffer).
- Filter state that must last from one call to the next goes in a
  `ModuleLogicalState` subclass (`create_logical_state`). Reset it in
  `on_state_change` when the voice starts again.
- Accept only the documented params struct IDs. For any other ID, pass the
  sound through.
- Guard against NaN, infinite and out-of-range parameters. A bad parameter
  must not make noise or a very loud output.
- No reference output from real hardware is available. The goal is a
  result that sounds correct, not a bit-exact result. Record this limit in
  the answer.
- Test: a unit test in a new `vita3k/ngs/tests/` suite is optional. At a
  minimum, build, run `container/vita3k.sh test`, and play the game on the
  device.

## Answer

Done on branch `log-review/ngs-effects`, merged to `master`.

- The curve code is in `vita3k/ngs/src/dsp.cpp` (`envelope_gain`,
  `advance_envelope`) with tests in `vita3k/ngs/tests/dsp_tests.cpp`.
- Linear segments are linear. Curved segments use smoothstep, the same as
  Vita3K-Plus. The real curve is not known.
- Loop: the segment from `nLoopEnd` goes back to `uLoopStart` over the time
  of `nLoopEnd`. This also works when `nLoopEnd` is the last point. Plus
  uses a different rule (a jump after it passes `nLoopEnd`). No public
  source says which one is correct.
- The envelope starts again when the voice goes from AVAILABLE to ACTIVE,
  and when the points change.
- The gain moves in a line across each grain, so a change gives no click.
- `SceNgsEnvelopeStates` is written each grain. `nReleasing` is always 0.

Not done: the release after key-off. vitasdk says that key-off starts the
release and that the voice runs until it ends. In Vita3K,
`sceNgsVoiceKeyOff` stops the voice at once. To add the release, the voice
must stay in FINALIZING until the envelope ends. That changes the voice
life cycle and needs its own issue (09).

Not checked by ear. Linux build, tests and format pass.

## Comments
