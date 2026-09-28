# 04: Add an adb test harness

Status: resolved
Claimed: 2026-09-25 Claude Code session (Opus 5.5)
Type: task
Label: ready-for-agent
Blocked by: none

## Goal

One script for the test loop on the device, so every run uses the same
commands, and a setting can change between A and B without a rebuild.

## Context

- Files on the device: `/sdcard/Android/data/<package>/files/` holds
  `vita3k.log`, `config.yml` and `config/config_<titleid>.xml`
  (`../map.md`, Notes). `adb pull` and `adb push` work there without
  `run-as`.
- `Emulator.java:171-185` starts a game from the intent extra `title_id`.
- The measured package is `org.vita3k.emulator` (release).

## Steps

Create `tools/android/device.sh`. Put usage text at the top, in the style
of `container/vita3k.sh`. Use `ANDROID_SERIAL` when set. Refuse to act when
more than one device is connected and no serial is given.

| Command | What it does |
|---|---|
| `info` | Print `ro.product.manufacturer`, `ro.product.model`, `ro.soc.model`, `ro.board.platform`, Android version, `uname -r`, each core's max frequency, display modes (`dumpsys display`), and thermal status (`dumpsys thermalservice`). |
| `install <apk>` | `adb install -r -t <apk>`. |
| `launch <package> <title id>` | Force-stop, then `am start -n <package>/org.vita3k.emulator.Emulator --es title_id <id>`. |
| `stop <package>` | Force-stop. |
| `release <package>` | Force-stop and delete `tmp/device.lock` if this session wrote it. |
| `log <package> <out dir>` | Pull `vita3k.log`. |
| `pull-perf <package> <out dir>` | Pull the files from ticket 05. |
| `config-get <package> <key>` | Pull `config.yml` and print the line for `<key>`. |
| `config-set <package> <key> <value>` | Stop the app, pull `config.yml`, change the one line, push it back, print the line. Fail if the key is not in the file. |
| `config-guard <package> <title id>` | Fail if `config/config_<title id>.xml` exists. |
| `keys <keycode>...` | Send `adb shell input keyevent` presses, 300 ms apart. |
| `thermal <out file>` | Once per second until stopped: thermal status, CPU temperatures, each core's current frequency, `/sys/class/kgsl/kgsl-3d0/gpuclk` and `gpu_busy_percentage`. Write CSV. Skip values that cannot be read, and say so once. |
| `screenshot <file>` | `screencap -p` on the device, `adb pull`, delete the device copy. |

## Acceptance

- `bash -n tools/android/device.sh` passes.
- On the device: `info`, `install`, `launch`, `log`, `config-set`,
  `config-guard`, `thermal` and `stop` work. Paste the `info` output under
  `## Answer`.
- Record whether `keys` presses reach a running game. They may not, because
  SDL can ignore events without an input device. If they do not, write that
  here, and ticket 06 must plan runs with the user.
- `CLAUDE.md` names the script in one line.

## Answer

Code done and merged to `master`: dfec4350 (merge 723db855).
`bash -n` passes. Each command was tested with a fake `adb` on the Mac:
`lock`, `release` (it keeps a lock of another session), `config-set` (it
fails on a missing key), `thermal` (it writes the header and one line per
second, and warns once about values it cannot read).

Extra command: `lock <ticket>` writes `tmp/device.lock` with the ticket and
the session name (`VITA3K_DEVICE_SESSION`, default `<user>@<host>`).

Still to do on the device (no device was connected on 2026-09-25):
`info` output, the device checks, and whether `keys` reaches a game.

Device test on 2026-09-28 (Pocket S, stock driver):

`info` output:

```
ro.product.manufacturer      AYANEO
ro.product.model             Pocket S
ro.soc.manufacturer          QTI
ro.soc.model                 SG8275
ro.board.platform            kalama
ro.build.version.release     13
ro.build.version.sdk         33
ro.build.id                  TKQ1.230811.002
uname -r                     5.15.104-android13-8-g05d70b033fc6

CPU max frequency (kHz):
  cpu0 2016000
  cpu1 2016000
  cpu2 2016000
  cpu3 2803200
  cpu4 2803200
  cpu5 2803200
  cpu6 2803200
  cpu7 3360000

Display modes:
  DisplayModeRecord{mMode={id=1, width=1440, height=2560, fps=60.000004, alternativeRefreshRates=[]}
  mActiveModeId=1

Thermal:
  Thermal Status: 0
```

- `install`, `launch`, `stop`, `release`, `log`, `config-get`,
  `config-set`, `config-guard`, `screenshot` and `thermal` work on the
  device.
- `keys` reaches a running game: `keys KEYCODE_BUTTON_START` left the
  Ratchet & Clank title screen.
- Fix befbcd94: after `adb push`, `config.yml` belongs to the shell user
  with mode 644. The app then fails at start with "Failed to initialise
  config". `config-set` now runs `chmod 666` after the push.
- Fix f16fd4cb: the device has 104 thermal zones. The old `thermal` read
  all zone types for every sample, so one line took about 10 seconds. Now
  one device loop finds the zones once. One line takes about 1.2 seconds.
- Note: `adb push` of a folder into the app folder fails with "remote
  secure_mkdirs failed". To restore folders, push to `/data/local/tmp/`
  and copy with `adb shell cp -r`. Then `chmod` the copied files to 666,
  or the app cannot write them.
