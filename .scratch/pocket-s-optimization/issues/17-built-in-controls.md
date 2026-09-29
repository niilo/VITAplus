# 17: Check the built-in controls and Ayaneo buttons

Status: open
Type: task
Label: ready-for-human
Blocked by: 04

## Goal

Every built-in control works in games, and one system button opens the
pause menu.

## Context

- SDL reads the gamepad. `InputDeviceUtils.kt:24-51` only sets the
  "controller connected" state in the UI; a skipped pad still reaches the
  game, but the UI shows no controller.
- The app handles `KEYCODE_BACK` and `KEYCODE_BUTTON_MODE` itself
  (`Emulator.java:301-327`). MODE opens the pause menu.
- The Pocket S has an AYASpace button, a Home button, a Turbo key and two
  extra shoulder buttons. The system may take some of them before SDL.

## Human steps (session H1)

With one game running, press each control and say what happens: D-pad,
both sticks, A, B, X, Y, L1, R1, L2, R2, L3, R3, Start, Select, the two
extra shoulder buttons, AYASpace, Home and Turbo.

## Agent steps

1. Run `adb shell getevent -lt` while the user presses each control.
   Record the device name, the key or axis code of each control, and
   whether the game reacted.
2. Check whether the built-in pad appears in the UI as a connected
   controller.
3. If a control does not work, or no button opens the pause menu, create a
   task ticket with the codes and the proposed fix.

## Answer

Note 2026-09-28: the app log says `1 Controllers Connected`,
`Controller 0: AYANEO Controller`. So SDL sees the built-in pad.
