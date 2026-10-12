#!/usr/bin/env bash
# Record the touches that walk a game from its title screen into gameplay.
#
# Usage: tools/android/touch_record.sh <package> <title_id> <label> [title]
#
#   <package>   org.vita3k.emulator or org.vita3k.emulator.debug
#   <title_id>  for example PCSE00052
#   <label>     a name for the recording, for example rayman-origins
#   title       the title as it should read in the report
#
# It launches the game, waits WAIT_BOOT seconds, then records the touchscreen
# until you press Enter. Play the game by hand from that point: touch to start,
# the menus, and into gameplay. Press Enter once you are in gameplay, or when
# you want to stop. The recording is then pulled and summarised.
#
# Results, under tmp/touch/<label>/:
#   events.txt  the raw getevent output, the source of truth
#   replay.sh   the device-side script that repeats the touches
#   touches.txt the touches as a list, to read without replaying
#   end.png     a screenshot taken when you pressed Enter
#
# Replay it with tools/android/touch_replay.sh.
#
# Why the raw values and not screen positions: a touch is recorded as the
# numbers the touchscreen reported, in its own coordinate system, and replayed
# unchanged. So the recording does not depend on the screen orientation, and
# the same file works for any title that uses the same menus.
#
# Environment:
#   WAIT_BOOT   seconds after launch before recording starts (default 50)
#   NODE        the touchscreen node (default /dev/input/event4)
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
repo_root="$(cd "$here/../.." && pwd)"
device="$here/device.sh"
package="org.vita3k.emulator.debug"
activity="org.vita3k.emulator.Emulator"
node="${NODE:-/dev/input/event4}"
remote_dir=/data/local/tmp/touch-record

if [[ $# -lt 3 ]]; then
    sed -n '2,/^set -euo/p' "$0" | sed '$d' | sed 's/^# \{0,1\}//'
    exit 1
fi
package="$1"
title_id="$2"
label="$3"
title="${4:-$title_id}"

out="$repo_root/tmp/touch/$label"
mkdir -p "$out"

wait_boot="${WAIT_BOOT:-50}"

if adb shell dumpsys window | grep -q 'isKeyguardShowing=true'; then
    echo "touch_record.sh: the device is locked. Unlock it first." >&2
    exit 1
fi

# The screen sleeps after about 30 s of nothing, and a recording is mostly
# waiting. Put it back when the script finishes.
prev_timeout="$(adb shell settings get system screen_off_timeout | tr -d '\r')"
restore_timeout() {
    [[ "$prev_timeout" =~ ^[0-9]+$ ]] && adb shell settings put system screen_off_timeout "$prev_timeout" > /dev/null 2>&1
}
trap restore_timeout EXIT
adb shell settings put system screen_off_timeout 1800000 > /dev/null 2>&1

echo "touch_record.sh: launching $title ($title_id), recording starts in ${wait_boot}s"
adb shell input keyevent KEYCODE_WAKEUP
adb shell am force-stop "$package"
adb shell am start -n "$package/$activity" --es title_id "$title_id" > /dev/null
sleep "$wait_boot"

# getevent writes to stdout with stdio buffering, which is block buffering when
# stdout is a file, so nothing would arrive until the buffer filled. awk with
# fflush puts every line out as it is read. stdin, stdout and stderr are taken
# off the adb connection so the shell does not wait on the pipeline.
adb shell "rm -rf '$remote_dir'; mkdir -p '$remote_dir'" > /dev/null
adb shell "nohup sh -c 'getevent -t $node 2>&1 | awk \"{ print; fflush() }\" > $remote_dir/events.txt' </dev/null >/dev/null 2>&1 &" > /dev/null 2>&1
sleep 3

lines="$(adb shell "wc -l < $remote_dir/events.txt 2>/dev/null | tr -d '\r'" || echo 0)"
echo "touch_record.sh: RECORDING now (${lines} lines so far)."
echo "touch_record.sh: play into gameplay by hand, then press Enter here."
read -r _

adb shell "pkill -f 'getevent -t $node'" > /dev/null 2>&1 || true
sleep 1
adb shell "pkill -f 'awk' " > /dev/null 2>&1 || true

adb pull "$remote_dir/events.txt" "$out/events.txt" > /dev/null
"$device" screenshot "$out/end.png" > /dev/null

count="$(grep -c . "$out/events.txt" || true)"
if [[ "${count:-0}" -lt 1 ]]; then
    echo "touch_record.sh: the recording is empty. No touch reached $node." >&2
    exit 1
fi

python3 "$here/touch_events.py" summarise "$out/events.txt" | tee "$out/touches.txt"
python3 "$here/touch_events.py" convert "$out/events.txt" "$out/replay.sh" --node "$node"

{
    echo "label: $label"
    echo "title: $title"
    echo "title_id: $title_id"
    echo "package: $package"
    echo "node: $node"
    echo "wait_boot: $wait_boot"
    echo "recorded: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
} > "$out/recording.txt"

echo "touch_record.sh: wrote $out/replay.sh. Check $out/end.png, then replay with:"
echo "  tools/android/touch_replay.sh $package $title_id $label"
