# Spec: optimize Vita3K for the Ayaneo Pocket S

## Goal

Make Vita3K run games on the Ayaneo Pocket S at a steadier and higher frame
rate than today, and make the measured best settings the default on this
device. Every change must be measured on the device. A change without a
measurement is not done.

## Target hardware

Sources: a Vulkan report from a real Pocket S on the stock driver
(<https://vulkan.gpuinfo.org/displayreport.php?id=30088>), the Qualcomm
product brief, and the Ayaneo product page. Rows marked "verify" are checked
in ticket 06.

| Part | Value |
|---|---|
| Model strings | `Build.MANUFACTURER` = `AYANEO`, `Build.MODEL` = `Pocket S` |
| SoC | Snapdragon G3x Gen 2, 4 nm. `ro.soc.manufacturer` = `QTI`, `ro.soc.model` = `SG8275`, `ro.board.platform` = `kalama` (checked 2026-09-28). |
| GPU | Adreno A32, up to 1 GHz. On the test device, the stock driver reports the Vulkan device name `Adreno (TM) 740` (checked 2026-09-28), not `Adreno (TM) A32` as in the gpuinfo report. The device ID is not logged yet. The gpuinfo report gives `0x43050A00`; the Adreno 740 is `0x43050A01`. |
| Stock driver | 512.676.0 (August 2023), Vulkan 1.3.128 |
| Turnip | Supports this chip ("FDA32") from Mesa 24.2.0. Older Turnip builds do not know it. |
| CPU | 8 cores. `cpuinfo_max_freq` (checked 2026-09-28): cpu0 to cpu2 at 2.016 GHz, cpu3 to cpu6 at 2.803 GHz, cpu7 (prime) at 3.36 GHz. So 3 small, 4 performance and 1 prime core. |
| RAM | LPDDR5X-8533, 12 GB or 16 GB. The test device has 16 GB. |
| Display | 6 inch IPS. 2560x1440 on the Advance model, 1920x1080 on the base model. The test device has one mode: 1440x2560 at 60 Hz. The panel is portrait in its native orientation; the app log says `native display rotation: ROTATION_90` (checked 2026-09-28). |
| Cooling | Vapour chamber and fan, 15 W sustained |
| Modes | Full Power (also called Max), Game, Balanced. Set with the Turbo key or in AYASpace. Power per mode is not published. |
| Input | Built-in gamepad, AYASpace button, Home button, Turbo key, two extra shoulder buttons. Key codes: ticket 17. |
| OS | Android 13 (build `TKQ1.230811.002`). No Android 14 update found. |

Facts that decide the plan:

- `VK_EXT_external_memory_host` is not offered by the stock driver or by
  Turnip on this GPU. The External Host mapping mode is not available.
- `VK_ANDROID_external_memory_android_hardware_buffer` is present, so the
  Native Buffer mode can work.
- BCn textures are supported by the stock driver without a patch.
- Present modes: MAILBOX and FIFO (plus shared modes). There is no
  IMMEDIATE.
- The Vita renders at 960x544. The resolution multiplier has 0.25 steps
  (`SettingsSections.kt:667`). 2.5x gives 2400x1360, the largest size under
  1440 lines. 2x gives 1920x1088.

## Success criteria

Ticket 06 records the baseline. Each benchmark title gets a target from
ticket 07: 30 or 60 FPS, the one that the profile shows is reachable.
Criteria are measured on a second play of the scene, with a warm shader
cache, in the Ayaneo mode named in ticket 06.

1. **Frame rate held.** At least 95% of recorded seconds have
   `fps >= target - 1`.
2. **Steady frames.** The 99th percentile of frame intervals (from
   `frames.csv`, ticket 05) is at most 1.5 times the target frame time.
3. **No heat drop.** In a 20 minute run, the average FPS of minutes 18 to 20
   is within 3% of the average of minutes 1 to 3, and the Android thermal
   status stays below `SEVERE` (4).
4. **No regression.** On every benchmark title, the average FPS is not lower
   than the baseline by more than the spread of the A runs.
5. **Preset.** On a Pocket S with no `config.yml`, the first launch writes
   the preset values. The user can still change each value.

A title that cannot meet 1 or 2 counts as a success if it improves over the
baseline by more than the A spread and meets 3 and 4. Record why.

## Measurement protocol

Every experiment uses this protocol. Record the results under the ticket's
`## Answer`.

1. **Package.** Use only the release package `org.vita3k.emulator`, built
   with `container/vita3k.sh android release`. Games, drivers and
   `config.yml` are per package. Never mix packages in one set of runs.
2. **Same build.** A and B use the same APK. Put a code change behind a
   temporary config value and switch it with `tools/android/device.sh
   config-set` (ticket 04). Compare two builds only when the ticket says
   that this is not possible.
3. **Device state.** Charger connected, battery at 50% or more, airplane
   mode on, brightness fixed at 50%, Ayaneo mode as named in ticket 06, run
   `adb shell am kill-all` before the set. Run `device.sh config-guard` to
   make sure no per-game config file overrides the test.
4. **Warm-up.** Do one run and discard it. It fills the shader cache.
   Tickets that test shader stutter say when to skip this step.
5. **Order.** Run A, B, A, B, A. Each run: reach the scene, wait 30 seconds,
   record 60 seconds. Between runs, stop the app and wait until the thermal
   status is 0 and the CPU temperature is within 2 °C of the first run's
   start temperature, or 3 minutes, whichever comes first.
6. **Validity.** The set is valid if the three A averages are within 3% of
   each other. B is better or worse only if its difference from the A mean
   is larger than the A spread (maximum minus minimum). If the set is not
   valid, repeat it once. If it is still not valid, write "not valid" and
   the numbers.
7. **Record.** Build commit, driver (stock or Turnip, with its file name),
   every setting that differs from the defaults, and the output of
   `tools/android/perf_summary.py` for each run.
8. **Raw files.** Keep logs, CSV files and screenshots under `tmp/` in the
   repository. Do not commit them.

## Constraints

- Do not change behavior on other devices unless it is measured there too.
  Device-specific defaults go through the preset in ticket 16.
- Match this device on `Build.MANUFACTURER`, `Build.MODEL` or the Vulkan
  device ID. Do not match GPU names that contain "740" or "7xx": the
  stock driver on this device reports `Adreno (TM) 740`, the same name as
  other devices.
- Build both targets before a commit: `container/vita3k.sh build` and
  `container/vita3k.sh android release`.
- Follow the writing standard in `CLAUDE.md` for all text.
- Users provide their own legally dumped games. Do not add game content to
  the repository.

## Out of scope

- Other Android devices, except as a regression check.
- Game-specific rendering bugs, unless a benchmark title cannot be measured
  without the fix.
- Rules and results for the AYN Thor. Results from the Thor are not proof
  for the Pocket S: the GPU ID and driver differ.
