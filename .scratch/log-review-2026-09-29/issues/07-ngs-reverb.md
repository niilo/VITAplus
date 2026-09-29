# 07: Implement the NGS reverb module

Status: open
Claimed: 2026-09-29 Claude Code session (Opus 5.5)
Type: task
Label: needs-info
Blocked by: 04

## Context

Uncharted logged `Game is using unimplemented reverb audio module` at
00:17:54. The params follow the I3DL2 reverb model (room, room HF, decay
time, reflections, reverb, delays, diffusion, density, HF reference).
Without reverb the sound is dry. That is not wrong, only plain.

## Plan

1. Implement an I3DL2-style reverb: early reflections from a short tapped
   delay line, and a late reverb from a feedback delay network, with
   decay from `fDecayTime` and HF damping from `fDecayHFRatio`. Levels
   are in millibels (I3DL2).
2. The reverb bus in most games has only wet signal. Check how Uncharted
   routes it (send level to a reverb rack) before the mix is decided.
3. Keep the CPU cost low. The target is the Ayaneo Pocket S. Measure the
   time per call on the device. If it is too slow, set Status to
   `rejected` and keep the passthrough.

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

Not implemented. The field names match I3DL2 (millibels, seconds, percent), and `fDryMB` suggests a wet plus dry output. No NGS source confirms either point.

What was done instead: the module logs the parameters that the game
sends, up to 16 distinct sets, at info level. The lines start with
`NGS reverb`. The sound still passes through with no change.
The helper is `is_new_param_set` in `vita3k/ngs/src/param_log.cpp`.

Next step: play Uncharted (and WipEout 2048) with `log-level: 2`, pull
`vita3k.log`, and read the values. For example, a `gain` near 1.0 means a
linear gain, and a `gain` from -12 to 12 means dB. Then decide the units
and implement the module. The research notes are in the session scratchpad (`ngs-research.md`). Their summary is in `spec.md`.

## Comments
