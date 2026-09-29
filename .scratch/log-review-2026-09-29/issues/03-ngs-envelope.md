# 03: Implement the NGS envelope module

Status: open
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

## Comments
