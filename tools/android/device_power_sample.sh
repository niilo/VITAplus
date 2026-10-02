#!/usr/bin/env bash
# Sample the device power draw and the clocks while a game plays.
#
# Usage: tools/android/device_power_sample.sh <out csv> <seconds>
#
# The target is energy, not frame rate, so every measurement needs this beside
# the perf-log CSVs. Device power is the battery current times the battery
# voltage:
#
#   /sys/class/power_supply/battery/current_now   microamps, negative while
#                                                  discharging
#   /sys/class/power_supply/battery/voltage_now   microvolts
#
# Their product is microwatts of the whole device: display, SoC, cores, GPU,
# radios. That is the number the user feels as battery life.
#
# power_now exists in the same folder but read 54450779 here, which is not a
# plausible device draw, so this script does not use it. The current times
# voltage product is what the charge accounting uses.
#
# The share of the draw that belongs to the emulator is NOT computed here: the
# display and the Android system are in the same total. Compare A and B measured
# with this script and the difference is the emulator share.
#
# Output columns:
#   second,power_uw,cpu0_khz,cpu3_khz,cpu7_khz,gpuclk_hz,gpu_busy_pct,
#   max_gpuclk_hz,throttling,temp_gpu_mdeg,temp_cpu1_mdeg,capacity_pct,
#   thermal_status
#
# thermal_status is sampled during the run, not read afterwards. A validity
# check that reads the status after the run cannot tell whether the run itself
# was thermally limited.
set -uo pipefail

out="${1:?usage: device_power_sample.sh <out csv> <seconds>}"
seconds="${2:-60}"

printf 'second,power_uw,cpu0_khz,cpu3_khz,cpu7_khz,gpuclk_hz,gpu_busy_pct,max_gpuclk_hz,throttling,temp_gpu_mdeg,temp_cpu1_mdeg,capacity_pct,thermal_status\n' > "$out"

for ((i = 0; i < seconds; i++)); do
    # One adb round trip for every numeric field, so the samples line up.
    # The payload holds no quotes: it sits inside a single-quoted string and a
    # quote inside it would close that string. gpu_busy_percentage reads as
    # "6 %", so the stray characters are stripped on the host below.
    line="$(adb shell 'B=/sys/class/power_supply/battery
        K=/sys/class/kgsl/kgsl-3d0
        C=/sys/devices/system/cpu
        cur=$(cat $B/current_now 2>/dev/null || echo 0)
        vol=$(cat $B/voltage_now 2>/dev/null || echo 0)
        echo $(( (cur * vol) / 1000 )) $(cat $C/cpu0/cpufreq/scaling_cur_freq 2>/dev/null || echo 0) $(cat $C/cpu3/cpufreq/scaling_cur_freq 2>/dev/null || echo 0) $(cat $C/cpu7/cpufreq/scaling_cur_freq 2>/dev/null || echo 0) $(cat $K/gpuclk 2>/dev/null || echo 0) $(cat $K/gpu_busy_percentage 2>/dev/null) $(cat $K/max_gpuclk 2>/dev/null || echo 0) $(cat $K/throttling 2>/dev/null || echo 0) $(cat /sys/class/thermal/thermal_zone63/temp 2>/dev/null || echo 0) $(cat /sys/class/thermal/thermal_zone40/temp 2>/dev/null || echo 0) $(cat $B/capacity 2>/dev/null || echo 0)' 2>/dev/null)"

    line="${line//$'\r'/}"   # adb appends a carriage return
    line="${line//%/}"        # and the busy percentage carries a percent sign
    # dumpsys thermalservice is a service call, so it cannot join the single
    # round trip above without making the payload much longer.
    tstat="$(adb shell dumpsys thermalservice 2>/dev/null | grep -m1 'Thermal Status' | tr -dc '0-9')"

    read -r raw_pw cpu0 cpu3 cpu7 gpuclk busy maxclk throttle temp_gpu temp_cpu cap <<< "$line"

    (( raw_pw < 0 )) && raw_pw=$(( -raw_pw ))   # negative while discharging

    printf '%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n' \
        "$i" "${raw_pw:-0}" "${cpu0:-0}" "${cpu3:-0}" "${cpu7:-0}" "${gpuclk:-0}" \
        "${busy:-0}" "${maxclk:-0}" "${throttle:-0}" "${temp_gpu:-0}" \
        "${temp_cpu:-0}" "${cap:-0}" "${tstat:-0}" >> "$out"
    sleep 1
done

# A short summary, because 60 rows of CSV is nobody's idea of a result.
awk -F, 'NR>1 && $2+0>0 {
    n++; s+=$2
    if (min==0 || $2+0<min) min=$2+0
    if ($2+0>max) max=$2+0
    if ($6+0>0) { gc+=$6; gn++ }
    if ($7+0>0) { gb+=$7; bn++ }
    if ($10+0>0) { tg+=$10; tn++ }
    if ($13+0>w) w=$13+0
} END {
    if (n==0) { print "no samples with a power reading"; exit }
    printf "power: mean %.3f W, min %.3f W, max %.3f W over %d s\n", s/n/1e6, min/1e6, max/1e6, n
    if (gn) printf "gpuclk: mean %.0f MHz\n", gc/gn/1e6
    if (bn) printf "gpu busy: mean %.1f percent\n", gb/bn
    if (tn) printf "gpu temp: mean %.1f C\n", tg/tn/1000
    if (w+0>0) printf "thermal status: worst %d\n", w
}' "$out" | tee "${out%.csv}-summary.txt"

echo "wrote $out"