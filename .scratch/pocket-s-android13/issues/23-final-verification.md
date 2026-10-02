# 23: Verify the result and write it down

Status: open
Type: task
Label: ready-for-human
Blocked by: 22, 07

## Steps

1. Build a release APK from `master`. Install it. Move `config.yml` away so the
   preset applies, then put it back at the end.
2. Run the ticket 04 benchmark on all four titles with the protocol, plus the
   20 minute run from ticket 21. Compare with the ticket 04 baseline.
3. Check each success criterion in `../spec.md` and mark it met or not met,
   with the numbers.
4. Run `container/vita3k.sh test` and `container/vita3k.sh format-check`. Then
   check one benchmark title on a desktop Linux build, because criterion 6 of
   `../spec.md` requires it and no other ticket does it.
5. Write the results up:
   - A short "Ayaneo Pocket S" section in `CLAUDE.md`: the recommended driver,
     what the preset sets, how to reset it, and the `setprop` command for any
     Turnip flag that ticket 19 kept. Under 20 lines.
   - One line per rejected ticket with its number, so nobody tries it again.
   - A "what is still slow" list, with the measured cost of each item, so the
     next plan starts from measurements instead of theories.
6. Close the old tickets that this plan replaces, with a pointer to the ticket
   that did the work:
   - `.scratch/pocket-s-optimization/issues/03-validation-layer-default.md`.
     The change is already in the base through Plus. Only the measurement was
     missing, and ticket 05 supplies it.
   - `.scratch/pocket-s-optimization/issues/05-frame-timing-log.md` (claimed)
     and `.scratch/plus-base/issues/04-perf-log.md`. Ticket 02 does this work.
     A ticket another session claimed closes with a pointer to that session's
     `## Answer`, not with a bare `resolved`.
   - `.scratch/pocket-s-optimization/issues/08-vblank-clock.md` (claimed).
     Ticket 10.
   - `.scratch/pocket-s-optimization/issues/09-dynarmic-flags.md` (open).
     Ticket 25.
   - `.scratch/pocket-s-optimization/issues/11-present-mode-vsync.md` (claimed).
     Ticket 09.
   - `.scratch/pocket-s-optimization/issues/16-device-preset.md` (open).
     Ticket 22.
   Leave the rest of `pocket-s-optimization` and `plus-base` alone. This plan
   does not cover the NGS work, the Adreno chain or the zip work.

## Answer

## Comments
