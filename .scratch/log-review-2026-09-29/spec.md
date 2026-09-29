# Log review, 2026-09-29

Source: `vita3k.log` from the Ayaneo Pocket S, pulled on 2026-09-29 with
`tools/android/device.sh log`. The session played Uncharted: Golden Abyss
(PCSA00029) and WipEout 2048 (PCSA00015). The build was
`niilo/pocket-s/11-present-mode-vsync` (4166-a52121af). Both games closed
without a crash.

The log showed three groups of small problems. Each group has an issue in
`issues/`:

- 01: five imports with no HLE function (`Import function for NID ... not
  found`).
- 02: `Using visibility index 808858157 which is too big for the buffer`.
- 03 to 07: NGS audio effect modules that do nothing (`Game is using
  unimplemented ... audio module`).
- 08: the device config has `log-level: trace`. Trace logs slow the games.
- 09: follow-up from 03. Key-off stops a voice at once, so the envelope
  release cannot play.
- 10: Uncharted shows artifacts at resolution multiplier 1. Multiplier 2
  has no artifacts. Fixed: a missing buffer barrier in the typeless copy.
- 11: Uncharted colors are duller than on Vita3K-Plus.

The crashes in `adb logcat -b crash` are from before 00:11. They are the
Uncharted firmware crash, which is solved (see
`.scratch/pocket-s-optimization/issues/06-baseline.md`).

## Research on the NGS effects

A research pass (2026-09-29) read only public sources: vitasdk `ngs.h`
(written from FW 3.60 reverse engineering), vitaAL (OpenAL on NGS by
GrapheneCt), Sly Cooper strings (Sony's Scream engine), a
minecraftpe-vita NGS header, Vita3K-Plus, and the I3DL2 docs. Results:

- Simple and Scream voices have 2 outputs ("sends"). SEND_1_FILTER
  probably acts on output 0 and SEND_2_FILTER on output 1. The EQ comes
  before the split (Scream names it "PRESEND_EQ").
- Filter frequency is in Hz (vitaAL). The units of resonance and gain, and
  the formulas, are not public.
- Compressor and distortion units are not public.
- Reverb field names match I3DL2. `fDryMB` suggests a dry level in mB.
- Key-off starts the envelope release, and the voice runs until it ends
  (vitasdk).
- No public code implements the filter, EQ, distortion or reverb.
