# 03: Turn the Vulkan validation layer off by default on Android

Status: superseded
Superseded by: the base already defaults `validation-layer` to false
Claimed: 2026-09-25 Claude Code session (Opus 5.5)
Type: task
Label: ready-for-agent
Blocked by: none
Measure after: 06

## Problem

`validation-layer` defaults to true (`config.h:136`, `EmulatorConfig.kt:81`).
Every APK carries `libVkLayer_khronos_validation.so` (`android/prebuilt/`),
and Android loads a layer packaged in the app's APK for release apps too.
When the driver offers a debug extension, the layer is enabled
(`vulkan/renderer.cpp:430-460`) and every Vulkan call is checked. This is
much slower.

## Steps

1. Take the change from `feat/ayaneo-pocket-s-performance` (`1838cd88`,
   the `CONFIG_DEFAULT_VALIDATION_LAYER` part of `config.h`) if ticket 02
   did not already bring it. Otherwise write it the same way: false on
   `__ANDROID__`, true elsewhere.
2. Set the Kotlin default in `EmulatorConfig.kt:81` to match.
3. Existing installs keep the value in their `config.yml`. Do not change
   that value in code. The preset (ticket 16) sets it for the Pocket S.

## Acceptance

- Both targets build.
- In ticket 06, the log of a default run on the device shows "Disabling
  Vulkan validation layers" with this change, and "Enabling vulkan
  validation layers" without it.
- Ticket 06 measures validation on against off. Copy that result under
  `## Answer`.

## Answer

Code done on branch `pocket-s/03-validation-layer-default`, commit
9f359dfa. Linux and Android release builds pass. Besides `config.h` and
`EmulatorConfig.kt`, the commit also changes the default in
`config/state.h` (`CurrentConfig`) and in `config/src/settings.cpp` (per-game
XML files without the attribute). Not merged: the measurement in ticket 06 is
still to do.

Update 2026-09-28: ticket 19 brought 1838cd88 and 64c694ae to `master`.
They make the same change: `config.h`, `state.h`, `EmulatorConfig.kt` and
the per-game XML default. A rebase of 9f359dfa on `master` leaves no
change, so the branch is not merged. The measurement in ticket 06 is still
to do.

Update 2026-10-03: this ticket is `superseded`. The plan it belongs to is
replaced by `.scratch/pocket-s-android13/`, and the goal already holds on the
Vita3K-Plus base without any of this work. `validation-layer` is false in
`vita3k/config/include/config/config.h:145`, in
`vita3k/config/include/config/state.h:106` and in
`vita3k/config/src/settings.cpp:252`. The measurement in ticket 06 is dropped
with the plan and will not be run, so nothing in `## Acceptance` is left to
meet.

## Comments

- 2026-10-03: closed as `superseded`. The line references in `## Problem`
  (`config.h:136`, `EmulatorConfig.kt:81`) were against the old base. On this
  base the setting is at `vita3k/config/include/config/config.h:145`.