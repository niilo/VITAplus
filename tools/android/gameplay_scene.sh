#!/usr/bin/env bash
# Reach Uncharted gameplay and measure while the camera moves.
#
# Usage: tools/android/gameplay_scene.sh <package> <label> [out dir]
#
#   <package>  org.vita3k.emulator or org.vita3kplus.emulator
#   <label>    a name for this run, for example stock-gameplay
#   out dir    default: tmp/gameplay/<label>
#
# Why this exists: the title screen of Uncharted barely touches the renderer,
# so a frame rate measured there says nothing about the game. This walks the
# menus into the saved chapter, then holds the right stick so the camera turns
# and the game has to render new geometry, textures and shaders every frame.
#
# The right stick is bound to I, J, K and L in the emulator config, so those
# are the keys sent. Movement is bound to W, A, S and D and is used as well,
# because moving the character changes the world the camera sees.
#
# With perf-log on, the run writes frames.csv, presents.csv and scenes.csv into
# the perf folder on the device. Pull them with device.sh pull-perf and read
# them with tools/android/perf_summary.py.
#
# Environment:
#   WAIT_BOOT     seconds after launch before the first tap (default 50)
#   WAIT_MENU     seconds after "Touch to Start" (default 9)
#   WAIT_DIALOG   seconds after "Continue" (default 4)
#   WAIT_LEVEL    seconds after "Yes", before measuring (default 55)
#   MOVE_SECONDS  how long to hold the camera and movement keys (default 45)
#   MOVE_KEYS     which keys to hold; default camera sweep plus movement
#   STEP_SHOTS    1 to save a screenshot after each step
#   PERF_LOG      1 to turn perf-log on before the run and off after (default 1)
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
repo_root="$(cd "$here/../.." && pwd)"
device="$here/device.sh"
title_id="PCSA00029"

[[ $# -ge 2 ]] || {
    sed -n '2,/^set -euo/p' "$0" | sed '$d' | sed 's/^# \{0,1\}//'
    exit 1
}
package="$1"
label="$2"
out="${3:-$repo_root/tmp/gameplay/$label}"
mkdir -p "$out"

wait_boot="${WAIT_BOOT:-50}"
wait_menu="${WAIT_MENU:-9}"
wait_dialog="${WAIT_DIALOG:-4}"
wait_level="${WAIT_LEVEL:-55}"
move_seconds="${MOVE_SECONDS:-45}"
move_keys="${MOVE_KEYS:-KEYCODE_I KEYCODE_K KEYCODE_D KEYCODE_W KEYCODE_J}"

if adb shell dumpsys window | grep -q 'isKeyguardShowing=true'; then
    echo "gameplay_scene.sh: the device is locked. Unlock it first." >&2
    exit 1
fi

# The screen sleeps after about 30 s of nothing, and a run spends most of its
# time waiting. Without this the device locks in the middle of a run and the
# game stops. The value is put back when the script finishes.
prev_timeout="$(adb shell settings get system screen_off_timeout | tr -d '\r')"
restore_timeout() {
    [[ "$prev_timeout" =~ ^[0-9]+$ ]] && adb shell settings put system screen_off_timeout "$prev_timeout" > /dev/null 2>&1
}
trap 'restore_timeout; '"$device"' release "$package" > /dev/null' EXIT
adb shell settings put system screen_off_timeout 1800000 > /dev/null 2>&1

# Touch positions are fractions of the landscape screen, the same convention
# uncharted_scene.sh uses. The game runs in landscape and the device reports
# the portrait order, so the longer side is the width.
size="$(adb shell wm size | tr -d '\r' | grep -o '[0-9]*x[0-9]*' | tail -n 1)"
side_a="${size%x*}"
side_b="${size#*x}"
if [[ "$side_a" -ge "$side_b" ]]; then width="$side_a"; height="$side_b"
else width="$side_b"; height="$side_a"; fi
at() { awk -v w="$width" -v h="$height" -v fx="$1" -v fy="$2" 'BEGIN { printf "%d %d", w * fx, h * fy }'; }

if [[ "${PERF_LOG:-1}" == 1 ]]; then
    "$device" config-set "$package" perf-log true > /dev/null
fi

step_shot() {
    [[ "${STEP_SHOTS:-0}" == 1 ]] && "$device" screenshot "$out/step-$1.png" > /dev/null || true
}

# Hold one key down. adb `input keyevent` sends a press and a release, which the
# game reads as one frame of movement. A hold needs a swipe with a long
# duration and no movement, which is what device.sh hold does.
hold_key() {
    local key="$1" ms="$2"
    # KEYCODE_I and friends are letters; the emulator maps letters through SDL.
    local code="${key#KEYCODE_}"
    adb shell input swipe "$(at 0.5 0.5)" "$(at 0.5 0.5)" 1 > /dev/null 2>&1 || true
    adb shell input keyevent --longpress "$key" > /dev/null 2>&1 &
    sleep "$(awk -v m="$ms" 'BEGIN { printf "%.2f", m / 1000 }')"
    wait || true
}

echo "gameplay_scene.sh: label=$label package=$package driver=$(  "$device" config-get "$package" custom-driver-name 2>/dev/null | tail -n 1)"

adb shell input keyevent KEYCODE_WAKEUP
"$device" launch "$package" "$title_id" > /dev/null
sleep "$wait_boot"
step_shot 1-title
"$device" hold $(at 0.5000 0.6250)   # Touch to Start
sleep "$wait_menu"
step_shot 2-menu
"$device" hold $(at 0.5000 0.5903)   # Continue
sleep "$wait_dialog"
step_shot 3-dialog
"$device" hold $(at 0.6484 0.8042)   # Yes
sleep "$wait_level"
step_shot 4-level

# Now in the saved chapter. Move the camera and the character so every frame
# brings new data.
echo "gameplay_scene.sh: in the level, moving for ${move_seconds}s"
started=$(date +%s)
i=0
while (( $(date +%s) - started < move_seconds )); do
    for key in $move_keys; do
        adb shell input keyevent "$key" > /dev/null 2>&1
        sleep 0.12
    done
    i=$((i + 1))
    if (( i % 20 == 0 )); then step_shot "move-$i"; fi
done
step_shot 5-after-move

"$device" screenshot "$out/scene.png" > /dev/null
"$device" pull-perf "$package" "$out" > /dev/null 2>&1 || true
"$device" log "$package" "$out" > /dev/null

{
    echo "package: $package"
    echo "label: $label"
    echo "screen: ${width}x${height}"
    echo "move_seconds: $move_seconds"
    echo "move_keys: $move_keys"
    for key in resolution-multiplier anisotropic-filtering memory-mapping custom-driver-name high-accuracy screen-filter; do
        "$device" config-get "$package" "$key" 2> /dev/null | tail -n 1 || true
    done
    grep -a -E 'driverID|Using the following memory mapping|renderer flags' "$out/vita3k.log" 2>/dev/null | tail -n 3 | cut -c1-170 || true
} | tee "$out/summary.txt"

if [[ -f "$out/frames.csv" ]]; then
    python3 "$here/perf_summary.py" "$out" --target 30 --warmup 5 | tee "$out/perf.txt"
fi

echo "Read $out/scene.png and check that step-4-level.png is the game and not a menu."