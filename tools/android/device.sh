#!/usr/bin/env bash
# Run the Vita3K test loop on an Android device through adb.
#
# Usage: tools/android/device.sh <command> [args]
#
#   info                              print device, CPU, display and thermal facts
#   install <apk>                     install or replace the APK
#   launch <package> <title id>       force-stop the app, then start the game
#   stop <package>                    force-stop the app
#   lock <ticket>                     write tmp/device.lock; fail if it exists
#   release <package>                 force-stop the app and delete tmp/device.lock
#                                     if this session wrote it
#   log <package> <out dir>           pull vita3k.log
#   pull-perf <package> <out dir>     pull the perf-log CSV files (setting perf-log)
#   config-get <package> <key>        print the config.yml line for <key>
#   config-set <package> <key> <val>  stop the app and set <key> in config.yml
#   config-guard <package> <title id> fail if a per-game config file exists
#   keys <keycode>...                 send key presses, 300 ms apart
#   thermal <out file>                write one CSV line per second until stopped:
#                                     thermal status, CPU temperatures, core
#                                     clocks, GPU clock and GPU busy percent
#   screenshot <file>                 take a screenshot and pull it
#
# Environment:
#   ANDROID_SERIAL          the device to use. Needed when more than one
#                           device is connected.
#   VITA3K_DEVICE_SESSION   the name written into tmp/device.lock
#                           (default: <user>@<host>)
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/../.." && pwd)"
lock_file="$repo_root/tmp/device.lock"
session="${VITA3K_DEVICE_SESSION:-$(id -un)@$(hostname -s)}"
activity="org.vita3k.emulator.Emulator"

die() {
    echo "device.sh: $*" >&2
    exit 1
}

usage() {
    sed -n '2,/^set -euo/p' "$0" | sed '$d' | sed 's/^# \{0,1\}//'
    exit "${1:-0}"
}

need_args() {
    local count="$1"
    shift
    [[ $# -ge $count ]] || usage 1
}

# Pick the device. Refuse to guess when more than one is connected.
check_device() {
    command -v adb > /dev/null || die "adb not found. Install the Android platform tools."
    [[ -n "${ANDROID_SERIAL:-}" ]] && return
    local devices
    devices="$(adb devices | awk 'NR > 1 && $2 == "device" { print $1 }')"
    local count
    count="$(printf '%s' "$devices" | grep -c . || true)"
    [[ "$count" -ge 1 ]] || die "no device connected"
    [[ "$count" -eq 1 ]] || die "more than one device connected. Set ANDROID_SERIAL."
    export ANDROID_SERIAL="$devices"
}

files_dir() {
    echo "/sdcard/Android/data/$1/files"
}

cmd_info() {
    local prop
    for prop in ro.product.manufacturer ro.product.model ro.soc.manufacturer ro.soc.model \
        ro.board.platform ro.build.version.release ro.build.version.sdk ro.build.id; do
        printf '%-28s %s\n' "$prop" "$(adb shell getprop "$prop" | tr -d '\r')"
    done
    printf '%-28s %s\n' "uname -r" "$(adb shell uname -r | tr -d '\r')"
    echo
    echo "CPU max frequency (kHz):"
    adb shell 'for c in /sys/devices/system/cpu/cpu[0-9]*; do
        f=$(cat $c/cpufreq/cpuinfo_max_freq 2>/dev/null || echo "?")
        echo "  ${c##*/} $f"
    done' | tr -d '\r'
    echo
    echo "Display modes:"
    adb shell dumpsys display | tr -d '\r' | grep -o 'DisplayModeRecord{[^}]*}\|mSupportedModes=[^]]*]\|mActiveModeId=[0-9]*\|renderFrameRate [0-9.]*' | sort -u | sed 's/^/  /'
    echo
    echo "Thermal:"
    adb shell dumpsys thermalservice | tr -d '\r' | grep -i -m 3 'status\|HAL Ready' | sed 's/^/  /'
}

cmd_launch() {
    local package="$1" title_id="$2"
    adb shell am force-stop "$package"
    adb shell am start -n "$package/$activity" --es title_id "$title_id"
}

cmd_lock() {
    local ticket="$1"
    mkdir -p "$(dirname "$lock_file")"
    if [[ -e "$lock_file" ]]; then
        die "the device is locked: $(tr '\n' ' ' < "$lock_file")"
    fi
    printf 'ticket=%s\nsession=%s\n' "$ticket" "$session" > "$lock_file"
    echo "locked for ticket $ticket by $session"
}

cmd_release() {
    local package="$1"
    adb shell am force-stop "$package"
    if [[ -e "$lock_file" ]]; then
        if grep -qx "session=$session" "$lock_file"; then
            rm "$lock_file"
            echo "lock released"
        else
            echo "device.sh: the lock belongs to another session. Not deleted." >&2
        fi
    fi
}

cmd_log() {
    local package="$1" out_dir="$2"
    mkdir -p "$out_dir"
    adb pull "$(files_dir "$package")/vita3k.log" "$out_dir/"
}

cmd_pull_perf() {
    local package="$1" out_dir="$2"
    mkdir -p "$out_dir"
    local remote
    remote="$(files_dir "$package")/perf"
    adb shell "[ -d '$remote' ]" || die "no perf files on the device. Turn on the setting perf-log first."
    adb pull "$remote/." "$out_dir/"
}

pull_config() {
    local package="$1" dest="$2"
    adb pull "$(files_dir "$package")/config.yml" "$dest" > /dev/null 2>&1
}

cmd_config_get() {
    local package="$1" key="$2" tmp
    tmp="$(mktemp)"
    pull_config "$package" "$tmp"
    grep -E "^$key:" "$tmp" || {
        rm -f "$tmp"
        die "key $key is not in config.yml"
    }
    rm -f "$tmp"
}

cmd_config_set() {
    local package="$1" key="$2" value="$3" tmp
    adb shell am force-stop "$package"
    tmp="$(mktemp)"
    pull_config "$package" "$tmp"
    if ! grep -qE "^$key:" "$tmp"; then
        rm -f "$tmp"
        die "key $key is not in config.yml"
    fi
    KEY="$key" VALUE="$value" awk '
        index($0, ENVIRON["KEY"] ":") == 1 { print ENVIRON["KEY"] ": " ENVIRON["VALUE"]; next }
        { print }' "$tmp" > "$tmp.new"
    adb push "$tmp.new" "$(files_dir "$package")/config.yml" > /dev/null 2>&1
    # A pushed file belongs to the shell user. The app must write config.yml
    # at start, or it fails with "Failed to initialise config".
    adb shell chmod 666 "$(files_dir "$package")/config.yml"
    rm -f "$tmp" "$tmp.new"
    cmd_config_get "$package" "$key"
}

cmd_config_guard() {
    local package="$1" title_id="$2" remote
    remote="$(files_dir "$package")/config/config_$title_id.xml"
    if adb shell "[ -e '$remote' ]"; then
        die "$remote exists and overrides config.yml. Move it away before the test."
    fi
    echo "no per-game config for $title_id"
}

cmd_keys() {
    local key
    for key in "$@"; do
        adb shell input keyevent "$key"
        sleep 0.3
    done
}

# One adb call for the whole run. The device script finds the CPU thermal
# zones once (there can be more than 100 zones, and a scan takes seconds),
# prints the header, then prints one line per second. A value that cannot be
# read is empty.
thermal_script='
zones=""
for z in /sys/class/thermal/thermal_zone*; do
    case "$(cat $z/type 2>/dev/null)" in
        *cpu*|*CPU*) zones="$zones $z" ;;
    esac
done
cpus=$(ls -d /sys/devices/system/cpu/cpu[0-9]* 2>/dev/null)
line="time_s,thermal_status"
for z in $zones; do line="$line,temp_$(cat $z/type)"; done
for c in $cpus; do line="$line,freq_${c##*/}"; done
echo "$line,gpuclk,gpu_busy_percentage"
while true; do
    status=$(dumpsys thermalservice 2>/dev/null | grep -m 1 -i "thermal status" | grep -o "[0-9][0-9]*" | head -n 1)
    line="$(date +%s),$status"
    for z in $zones; do line="$line,$(cat $z/temp 2>/dev/null)"; done
    for c in $cpus; do line="$line,$(cat $c/cpufreq/scaling_cur_freq 2>/dev/null)"; done
    gpuclk=$(cat /sys/class/kgsl/kgsl-3d0/gpuclk 2>/dev/null)
    busy=$(cat /sys/class/kgsl/kgsl-3d0/gpu_busy_percentage 2>/dev/null | tr -d " %")
    echo "$line,$gpuclk,$busy"
    # The reads above take about 0.5 s.
    sleep 0.5
done
'

cmd_thermal() {
    local out_file="$1" line warned=0
    mkdir -p "$(dirname "$out_file")"
    : > "$out_file"
    echo "writing $out_file. Stop with Ctrl-C."
    adb shell "sh -c '$thermal_script'" | tr -d '\r' | while IFS= read -r line; do
        echo "$line" >> "$out_file"
        if [[ "$warned" -eq 0 && ("$line" == *,,* || "$line" == *,) ]]; then
            echo "device.sh: some values cannot be read. They are empty in the CSV." >&2
            warned=1
        fi
    done
}

cmd_screenshot() {
    local file="$1" remote="/sdcard/vita3k-screenshot.png"
    mkdir -p "$(dirname "$file")"
    adb shell screencap -p "$remote"
    adb pull "$remote" "$file" > /dev/null
    adb shell rm "$remote"
    echo "$file"
}

[[ $# -ge 1 ]] || usage 1
command="$1"
shift

case "$command" in
    -h | --help | help) usage 0 ;;
    lock)
        need_args 1 "$@"
        cmd_lock "$1"
        exit 0
        ;;
esac

check_device
case "$command" in
    info) cmd_info ;;
    install)
        need_args 1 "$@"
        adb install -r -t "$1"
        ;;
    launch)
        need_args 2 "$@"
        cmd_launch "$1" "$2"
        ;;
    stop)
        need_args 1 "$@"
        adb shell am force-stop "$1"
        ;;
    release)
        need_args 1 "$@"
        cmd_release "$1"
        ;;
    log)
        need_args 2 "$@"
        cmd_log "$1" "$2"
        ;;
    pull-perf)
        need_args 2 "$@"
        cmd_pull_perf "$1" "$2"
        ;;
    config-get)
        need_args 2 "$@"
        cmd_config_get "$1" "$2"
        ;;
    config-set)
        need_args 3 "$@"
        cmd_config_set "$1" "$2" "$3"
        ;;
    config-guard)
        need_args 2 "$@"
        cmd_config_guard "$1" "$2"
        ;;
    keys)
        need_args 1 "$@"
        cmd_keys "$@"
        ;;
    thermal)
        need_args 1 "$@"
        cmd_thermal "$1"
        ;;
    screenshot)
        need_args 1 "$@"
        cmd_screenshot "$1"
        ;;
    *) usage 1 ;;
esac
