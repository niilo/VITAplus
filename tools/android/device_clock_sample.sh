#!/usr/bin/env bash
# Sample the clocks of one run while a game is playing.
#
# Usage: tools/android/device_clock_sample.sh <out csv> <seconds>
#
# The paths come from ticket 01 of .scratch/pocket-s-android13/, which read
# them off the device. gpuclk is in Hz, cpu scaling_cur_freq is in kHz, and
# the temperature zones that read are named cpu-0-* and cpu-1-*.
set -uo pipefail

out="${1:?usage: device_clock_sample.sh <out csv> <seconds>}"
seconds="${2:-60}"

samples=$seconds
printf 'second,gpuclk_hz,gpu_busy_pct,max_gpuclk_hz,throttling,cpu0_khz,cpu3_khz,cpu7_khz,temp_cpu0_mdeg,temp_cpu1_mdeg\n' > "$out"

for ((i = 0; i < samples; i++)); do
    read -r gpuclk busy maxclk throttle cpu0 cpu3 cpu7 < <(adb shell '
        kg=/sys/class/kgsl/kgsl-3d0
        cp=/sys/devices/system/cpu
        echo "$(cat $kg/gpuclk 2>/dev/null) $(cat $kg/gpu_busy_percentage 2>/dev/null | tr -d " %") $(cat $kg/max_gpuclk 2>/dev/null) $(cat $kg/throttling 2>/dev/null) $(cat $cp/cpu0/cpufreq/scaling_cur_freq 2>/dev/null) $(cat $cp/cpu3/cpufreq/scaling_cur_freq 2>/dev/null) $(cat $cp/cpu7/cpufreq/scaling_cur_freq 2>/dev/null)"' | tr -d '\r')

    temps=$(adb shell 'for z in /sys/class/thermal/thermal_zone*; do t=$(cat $z/type 2>/dev/null); case "$t" in cpu-0-*) printf "%s " "$(cat $z/temp 2>/dev/null)";; esac; done' | tr -d '\r')
    t0=$(echo "$temps" | awk '{print $1}')
    t1=$(echo "$temps" | awk '{print $4}')
    printf '%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n' "$i" "$gpuclk" "$busy" "$maxclk" "$throttle" "$cpu0" "$cpu3" "$cpu7" "$t0" "$t1" >> "$out"
    sleep 1
done

echo "wrote $out"