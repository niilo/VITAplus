#!/usr/bin/env bash
# Start Uncharted: Golden Abyss, load the saved chapter and sample the FPS counter.
# Use it to compare builds, drivers and settings in the same scene.
#
# Usage: tools/android/uncharted_scene.sh <package> <label> [out dir]
#
#   <package>  org.vita3k.emulator or org.vita3kplus.emulator
#   <label>    a name for this run, for example turnip-res2
#   out dir    default: tmp/uncharted-scene/<label>
#
# It needs a save game in slot 0 of PCSA00029 and an unlocked device. It walks
# through the menus with held touches (see `device.sh hold`): "Touch to Start",
# "Continue", "Yes". The positions are fractions of the screen, measured on the
# Ayaneo Pocket S (2560 x 1440). A cold start with a custom driver needs about
# 50 seconds before the title screen takes touches. Times can be changed with the environment
# variables WAIT_BOOT (50 s), WAIT_MENU (9 s), WAIT_DIALOG (4 s) and WAIT_LEVEL
# (55 s). With STEP_SHOTS=1 it also saves a screenshot after each step
# (step-1-title.png and so on), to see where a run went wrong.
#
# Result in the out dir:
#   fps.png      six crops of the FPS counter, 2 s apart. Read the numbers.
#   scene.png    a screenshot of the scene. Check that it is the level.
#   vita3k.log   the log of the run
#   summary.txt  the driver, the memory mapping and the main settings
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
out="${3:-$repo_root/tmp/uncharted-scene/$label}"
mkdir -p "$out"

wait_boot="${WAIT_BOOT:-50}"
wait_menu="${WAIT_MENU:-9}"
wait_dialog="${WAIT_DIALOG:-4}"
wait_level="${WAIT_LEVEL:-55}"

if adb shell dumpsys window | grep -q 'isKeyguardShowing=true'; then
    echo "uncharted_scene.sh: the device is locked. Unlock it first." >&2
    exit 1
fi

# Screen size, for example "Physical size: 1440x2560". The device reports the
# natural (portrait) order, but the game runs in landscape and touch positions
# use the landscape order, so the longer side is the width.
size="$(adb shell wm size | tr -d '\r' | grep -o '[0-9]*x[0-9]*' | tail -n 1)"
side_a="${size%x*}"
side_b="${size#*x}"
if [[ "$side_a" -ge "$side_b" ]]; then
    width="$side_a"
    height="$side_b"
else
    width="$side_b"
    height="$side_a"
fi
size="${width}x${height}"
at() { awk -v w="$width" -v h="$height" -v fx="$1" -v fy="$2" 'BEGIN { printf "%d %d", w * fx, h * fy }'; }

"$device" lock "uncharted-scene-$label" > /dev/null
trap '"$device" release "$package" > /dev/null' EXIT

step_shot() {
    [[ "${STEP_SHOTS:-0}" == 1 ]] && "$device" screenshot "$out/step-$1.png" > /dev/null || true
}

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

"$device" screenshot "$out/scene.png" > /dev/null
python3 "$here/fps_sample.py" "$out/fps.png" --count 6 --interval 2 > /dev/null
"$device" log "$package" "$out" > /dev/null

{
    echo "package: $package"
    echo "label: $label"
    echo "screen: $size"
    for key in resolution-multiplier anisotropic-filtering memory-mapping custom-driver-name; do
        "$device" config-get "$package" "$key" 2> /dev/null | tail -n 1 || true
    done
    grep -a -E 'Custom Adreno driver|Stock Adreno driver|Using the following memory mapping' "$out/vita3k.log" | tail -n 2 | cut -c1-160 || true
} | tee "$out/summary.txt"
echo "Read $out/fps.png and check $out/scene.png."
