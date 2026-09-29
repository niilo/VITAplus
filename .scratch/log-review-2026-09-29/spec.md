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

The crashes in `adb logcat -b crash` are from before 00:11. They are the
Uncharted firmware crash, which is solved (see
`.scratch/pocket-s-optimization/issues/06-baseline.md`).
