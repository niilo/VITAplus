#!/usr/bin/env bash
# Launch a game and replay a recorded touch walk into it.
#
# Usage: tools/android/touch_replay.sh <package> <title_id> <label>
#
#   <package>   org.vita3k.emulator or org.vita3k.emulator.debug
#   <title_id>  for example PCSE00052
#   <label>     the recording under tmp/touch/<label>
#
# It launches the game, waits the same WAIT_BOOT the recording used, then runs
# the recorded touches on the device. The timing runs on the device, so the
# replay is not slowed by one adb call per event.
#
# Results, under tmp/touch/<label>/replay-<stamp>/:
#   scene.png   a screenshot after the walk, to check that it reached gameplay
#   vita3k.log the log of the run
#   summary.txt the settings the run used and the driverID from the log
#
# This is the check that the walk still works. A replay that ends on a menu
# means the walk is stale, not that the game is broken: the recording has to be
# made again, or the wait has to be longer.
#
# Environment:
#   WAIT_BOOT   seconds after launch before the replay (default: the value in
#               the recording, otherwise 50)
#   SPEED       divide every gap between touches (default 1)
#   SETTLE      seconds to wait after the walk before the screenshot (default 5)
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
repo_root="$(cd "$here/../.." && pwd)"
device="$here/device.sh"
activity="org.vita3k.emulator.Emulator"
remote_script=/data/local/tmp/touch-replay.sh

if [[ $# -lt 3 ]]; then
    sed -n '2,/^set -euo/p' "$0" | sed '$d' | sed 's/^# \{0,1\}//'
    exit 1
fi
package="$1"
title_id="$2"
label="$3"

record_dir="$repo_root/tmp/touch/$label"
script="$record_dir/replay.sh"
[[ -s "$script" ]] || {
    echo "touch_replay.sh: no recording at $script. Run tools/android/touch_record.sh first." >&2
    exit 1
}

wait_boot="${WAIT_BOOT:-$(awk '/^wait_boot:/ { print $2 }' "$record_dir/recording.txt" 2>/dev/null || echo 50)}"
[[ "$wait_boot" =~ ^[0-9]+$ ]] || wait_boot=50
speed="${SPEED:-1}"
settle="${SETTLE:-5}"
stamp="$(date -u +%Y%m%dT%H%M%SZ)"
out="$record_dir/replay-$stamp"
mkdir -p "$out"

if adb shell dumpsys window | grep -q 'isKeyguardShowing=true'; then
    echo "touch_replay.sh: the device is locked. Unlock it first." >&2
    exit 1
fi

prev_timeout="$(adb shell settings get system screen_off_timeout | tr -d '\r')"
restore_timeout() {
    [[ "$prev_timeout" =~ ^[0-9]+$ ]] && adb shell settings put system screen_off_timeout "$prev_timeout" > /dev/null 2>&1
}
trap 'restore_timeout; '"$device"' release "$package" > /dev/null' EXIT
adb shell settings put system screen_off_timeout 1800000 > /dev/null 2>&1

# The script on the device is regenerated when a speed is asked for, so the
# default case pushes the recording as it is.
if [[ "$speed" == 1 ]]; then
    adb push "$script" "$remote_script" > /dev/null
else
    tmp_script="$out/replay-speed$speed.sh"
    python3 "$here/touch_events.py" convert "$record_dir/events.txt" "$tmp_script" --speed "$speed"
    adb push "$tmp_script" "$remote_script" > /dev/null
fi

echo "touch_replay.sh: launching $title_id, replay starts in ${wait_boot}s"
adb shell input keyevent KEYCODE_WAKEUP
adb shell am force-stop "$package"
adb shell am start -n "$package/$activity" --es title_id "$title_id" > /dev/null
sleep "$wait_boot"

echo "touch_replay.sh: replaying $label (speed $speed)"
adb shell "sh $remote_script" > /dev/null 2>&1 || true
sleep "$settle"

"$device" screenshot "$out/scene.png" > /dev/null
"$device" log "$package" "$out" > /dev/null

{
    echo "label: $label"
    echo "title_id: $title_id"
    echo "package: $package"
    echo "wait_boot: $wait_boot"
    echo "speed: $speed"
    echo "replayed: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
    for key in resolution-multiplier memory-mapping custom-driver-name high-accuracy ngs-enable; do
        "$device" config-get "$package" "$key" 2> /dev/null | tail -n 1 || true
    done
    # The requested driver name is not the driver that ran, so take the name
    # from the log. A run without this line says nothing about the driver.
    driver_id="$(grep -a -oE 'driverID: [A-Za-z]+' "$out/vita3k.log" 2>/dev/null | tail -n 1 | cut -d' ' -f2)"
    echo "driverID: ${driver_id:-UNKNOWN}"
} | tee "$out/summary.txt"

echo "touch_replay.sh: read $out/scene.png and decide whether it is gameplay, not a menu."
