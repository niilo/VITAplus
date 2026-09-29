# Move the fork to the Vita3K-Plus code base

Decision, 2026-09-29: the user answered "1" to this question: switch to
Plus as the base (1), or keep our code and keep picking Plus fixes (2).
This replaces the answer of `.scratch/pocket-s-optimization/issues/02`.

Evidence: on the Ayaneo Pocket S, Uncharted (PCSA00029) at resolution
multiplier 2, Turnip, double-buffer mapping, the same settings in both
apps: Plus 20588fbf has no black boxes, better colors, and 30 FPS. Our
`master` 5f122052 has black boxes and 18 FPS. At multiplier 1, Plus fixed
the flickering dashes that took a full bisect to find
(`.scratch/log-review-2026-09-29/issues/10`).

The new base is `plus/all-enhancements` 20588fbf. Our own work goes on
top. The old `master` stays as the branch `pre-plus-master`.

Our commits since upstream bbd5c362, in groups:

| Group | Action |
| --- | --- |
| Tools and docs (CLAUDE.md, AGENTS.md, docs/, container/, tools/android/device.sh, .scratch/) | Take all |
| Plus picks (a4e6c5be, b883600f, 2e5e8248, d76436f0, c832a8aa, 148c07fd, 6839971c, 4bb31060, f9510fac, 8479ac23, 132fce5f, 8de1c608) | Skip, Plus has them |
| Our fixes of Plus code (92df8755, bb44d95c, bf32c3b6) | Check; Plus has the first two |
| Our features (9936aacd, 5cfb95a4, the NGS switches, d392779e perf-log) | Port and fit to Plus |
| NGS envelope and compressor (b258b9de) | Keep the Plus versions; port only dsp.h/dsp.cpp and tests |
| Adreno chain (682bd5e3 to 76870af3, 53fc38db, a778c289) | Separate ticket: measure first |
| Other (a32b6aed GL hashless cache, 96d8d71d debug key) | Check if Plus has them |
