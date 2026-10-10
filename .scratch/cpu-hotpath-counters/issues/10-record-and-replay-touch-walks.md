# 10: Record a touch walk once and replay it into any title

Status: open
Type: task
Label: ready-for-human

## Goal

`tools/android/gameplay_scene.sh` walks one game: Uncharted, PCSA00029,
through hardcoded coordinates in its menu. Every other title needs a new
script with new coordinates, and one that nobody has to write cannot be
measured at all.

Record the touches once, by hand, into any title, and replay them. The
recording is the input and the walk becomes data instead of code.

## What is in the tree

Four files, untracked, with the unit tests passing:

- `tools/android/touch_record.sh <package> <title_id> <label> [title]`.
  Launches the title, waits `WAIT_BOOT` seconds (default 50), then records
  the touchscreen with `getevent -t` until Enter is pressed. Pulls the raw
  events to `tmp/touch/<label>/events.txt`, takes a screenshot, then calls
  `touch_events.py summarise` and `convert`.
- `tools/android/touch_replay.sh <package> <title_id> <label>`. Launches the
  title, waits the same boot time, and runs the generated script on the
  device. Writes `scene.png`, `vita3k.log` and `summary.txt` under
  `tmp/touch/<label>/replay-<stamp>/`.
- `tools/android/touch_events.py`. `convert` writes the device-side script,
  `summarise` prints one line per touch.
- `tools/android/test_touch_events.py`. 19 tests, all pass.

## Design decisions already made

**Record raw values, not screen positions.** A touch is recorded as the
numbers the touchscreen reported, in its own coordinate system, and is
replayed unchanged. The recording does not depend on the screen
orientation, and the same file works for any title whose menus are at the
same place. The alternative, mapping to screen fractions as
`gameplay_scene.sh` does, needs a new script per resolution.

**Time the gaps on the device.** `sendevent` per event over adb would add
the round trip to every event. The generated script sleeps and sends on the
device, so a replay runs at the recorded speed.

**Only touchscreen events are replayed.** `touch_events.py` keeps `EV_ABS`
codes for the multitouch slots and positions, `EV_SYN` reports and `EV_KEY`.
A recording that picked up another input device sends none of its events.

**`--speed` divides every gap.** It scales the sleeps in the generated
script, so the script needs no speed argument on the device.

## What is not done

Nothing has run on a device. The unit tests cover the parser, the
`signed_32` conversion of a `getevent` tracking id of `ffffffff`, the
grouping and the touch extraction. They do not cover `getevent` output
from this device, because no recording of it exists in the tree.

## Steps for a person with the device

1. `tools/android/touch_record.sh org.vita3k.emulator.debug PCSA00080 jak <title>`.
   Unlock the device first; the script refuses while the keyguard is up.
   Play into gameplay by hand, then press Enter.
2. Read `tmp/touch/jak/touches.txt` and `end.png`. A recording that never
   reaches gameplay is a recording of the wrong walk.
3. `tools/android/touch_replay.sh org.vita3k.emulator.debug PCSA00080 jak`.
   Read `scene.png` and `summary.txt`.
4. Record `driverID` from `summary.txt`. `summary.txt` names the driver
   that was asked for, which is not the driver that ran. See
   `.scratch/pocket-s-android13/issues/04-baseline.md`.

## Acceptance

- One recording of the Jak in-game walk exists under `tmp/touch/`, and
  `touch_replay.sh` reproduces it into gameplay. `scene.png` shows
  gameplay and not a menu.
- `summary.txt` carries a `driverID` that is not `UNKNOWN`.
- `python3 tools/android/test_touch_events.py` passes.
- `CLAUDE.md` names the two tools next to `gameplay_scene.sh`, so the next
  person does not write another per-title script.

## Comments