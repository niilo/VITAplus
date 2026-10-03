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
#   hold <x> <y> [ms]                 hold a touch at a screen position (default
#                                     300 ms). A plain tap is too short for the
#                                     games, and key events do not reach them.
#   perf <package> <label> [secs]     record simpleperf against the app, pull the
#                                     symbols, perf.data and an HTML report into
#                                     tmp/perf/<label>/
#   trace <package> <label> <secs>    record a Perfetto trace with the sched,
#                                     freq, gfx, power and thermal events, then
#                                     pull it into tmp/trace/<label>/
#   clocks <package> <label> [secs]   sample the GPU and CPU clocks, the GPU busy
#                                     percentage and the temperature into
#                                     tmp/clocks/<label>/clocks.csv. This is the
#                                     thermal command under its new name: one
#                                     command, so there is one set of sysfs paths.
#                                     The package is accepted so every command has
#                                     the same shape; the clocks are the device's.
#   latency <package> <label> [secs]  sample dumpsys SurfaceFlinger --latency into
#                                     tmp/latency/<label>/latency.csv
#   screenshot <file>                 take a screenshot and pull it
#   baseline <package> <title id> <label> [key=value]...
#                                     run one measurement set for one title: a
#                                     discarded warm-up, then A, B, A, B, A. Each
#                                     run samples the power and the
#                                     SurfaceFlinger latency beside it and writes
#                                     tmp/baseline/<label>/report.txt. A is the
#                                     reference and carries no setting; B applies
#                                     the key=value pairs, so A and B differ only
#                                     in the setting and use the same APK.
#
# Environment:
#   ANDROID_SERIAL          the device to use. Needed when more than one
#                           device is connected.
#   VITA3K_DEVICE_SESSION   the name written into tmp/device.lock
#                           (default: <user>@<host>)
#   VITA3K_UNSTRIPPED_LIB   path to the unstripped libVita3K.so for `perf`.
#                           Without it the script looks under
#                           android/app/build/intermediates/cxx.
#   VITA3K_PERF_SECONDS     default seconds for `perf` (default 30)
#   BASELINE_SECONDS        recorded seconds per run (default 60)
#   BASELINE_WARMUP         seconds of gameplay the report skips (default 120)
#   BASELINE_SETTLE         longest cooldown wait between runs, seconds (180)
#   BASELINE_TARGET         target FPS for the late-frame fraction (default 30)
#   BASELINE_LEAD           seconds before the gameplay starts, for a scene script
#                           other than gameplay_scene.sh. Left unset, the value is
#                           read out of the scene script, so the samplers cover
#                           the movement and not the menu walk.
#   BASELINE_SCENE          scene script for a title other than Uncharted. It is
#                           called as <script> <package> <label> <out dir>, the
#                           same shape as gameplay_scene.sh.
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
    # A key that an older build did not write goes before the "..." line
    # that ends the YAML document. After that line it would be ignored.
    local missing=0
    grep -qE "^$key:" "$tmp" || missing=1
    KEY="$key" VALUE="$value" MISSING="$missing" awk '
        index($0, ENVIRON["KEY"] ":") == 1 { print ENVIRON["KEY"] ": " ENVIRON["VALUE"]; next }
        $0 == "..." && ENVIRON["MISSING"] == "1" { print ENVIRON["KEY"] ": " ENVIRON["VALUE"]; added = 1 }
        { print }
        END { if (ENVIRON["MISSING"] == "1" && !added) print ENVIRON["KEY"] ": " ENVIRON["VALUE"] }' "$tmp" > "$tmp.new"
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

cmd_hold() {
    local x="$1" y="$2" ms="${3:-300}"
    adb shell input swipe "$x" "$y" "$x" "$y" "$ms"
}

cmd_keys() {
    local key
    for key in "$@"; do
        adb shell input keyevent "$key"
        sleep 0.3
    done
}

# One adb call for every field, so the samples of a run line up. The paths come
# from ticket 01 of .scratch/pocket-s-android13/, which read them off the device:
#
#   /sys/class/kgsl/kgsl-3d0/gpuclk                 GPU clock in Hz
#   /sys/class/kgsl/kgsl-3d0/gpu_busy_percentage    percent, with a % sign
#   /sys/class/kgsl/kgsl-3d0/max_gpuclk             the cap, in Hz
#   /sys/class/kgsl/kgsl-3d0/throttling             nonzero when the GPU is limited
#   /sys/devices/system/cpu/cpu*/cpufreq/scaling_cur_freq    in kHz
#   /sys/devices/system/cpu/cpu*/cpufreq/cpuinfo_max_freq    in kHz
#
# gpuclk_khz and busclk_khz do not exist on this kernel. There is no
# /sys/devices/system/cpu/policy* either, and cpuinfo_cur_freq is permission
# denied.
#
# The temperature zones that read without root are named cpu-0-* and cpu-1-*. The
# zones named pa, sdr*, mmw*, epm* and pmr735d* return "Invalid argument" and are
# skipped. temp_cpu_mdeg is the maximum over the cpu-0-* zones and
# temp_cpu1_mdeg the maximum over the cpu-1-* zones, which is what the cooldown
# step of the measurement protocol needs.
#
# This replaces the old thermal command. One command means one set of paths,
# because the two disagreed about whether the Qualcomm cpufreq files live under
# cpu*/cpufreq or policy*/cpufreq and only one set is right.
clocks_script='
kg=/sys/class/kgsl/kgsl-3d0
cp=/sys/devices/system/cpu
c0=""
c1=""
for z in /sys/class/thermal/thermal_zone*; do
    t=$(cat $z/type 2>/dev/null)
    case "$t" in
        cpu-0-*)
            v=$(cat $z/temp 2>/dev/null)
            case "$v" in ""|*[!0-9]*) continue ;; esac
            if [ -z "$c0" ] || [ "$v" -gt "$c0" ]; then c0=$v; fi
            ;;
        cpu-1-*)
            v=$(cat $z/temp 2>/dev/null)
            case "$v" in ""|*[!0-9]*) continue ;; esac
            if [ -z "$c1" ] || [ "$v" -gt "$c1" ]; then c1=$v; fi
            ;;
    esac
done
echo "$(date +%s),$(cat $kg/gpuclk 2>/dev/null),$(cat $kg/gpu_busy_percentage 2>/dev/null | tr -d " %"),$(cat $kg/max_gpuclk 2>/dev/null),$(cat $kg/throttling 2>/dev/null),$(cat $kg/temp 2>/dev/null),$(cat $cp/cpu0/cpufreq/scaling_cur_freq 2>/dev/null),$(cat $cp/cpu0/cpufreq/cpuinfo_max_freq 2>/dev/null),$(cat $cp/cpu3/cpufreq/scaling_cur_freq 2>/dev/null),$(cat $cp/cpu3/cpufreq/cpuinfo_max_freq 2>/dev/null),$(cat $cp/cpu7/cpufreq/scaling_cur_freq 2>/dev/null),$(cat $cp/cpu7/cpufreq/cpuinfo_max_freq 2>/dev/null),$c0,$c1"
'

clocks_header='second,gpuclk_hz,gpu_busy_pct,max_gpuclk_hz,throttling,temp_gpu_mdeg,cpu0_khz,cpu0_max_khz,cpu3_khz,cpu3_max_khz,cpu7_khz,cpu7_max_khz,temp_cpu0_mdeg,temp_cpu1_mdeg'

cmd_clocks() {
    local label="$1" seconds="${2:-60}"
    local out_file="$repo_root/tmp/clocks/$label/clocks.csv"
    mkdir -p "$(dirname "$out_file")"
    echo "$clocks_header" > "$out_file"

    local i line
    local -a header
    IFS=',' read -r -a header <<< "$clocks_header"
    # The empty columns that follow the index. A row for a sample that returned
    # nothing keeps all 14 fields, so a gap cannot shift the columns after it.
    local blanks=""
    local col
    for ((col = 1; col < ${#header[@]}; col++)); do blanks="$blanks,"; done

    for ((i = 0; i < seconds; i++)); do
        # Every stage carries `|| true`. With `set -e` and `pipefail` an adb
        # failure here would end the script with no message at all, and the
        # empty case is handled below instead.
        line="$({ adb shell "sh -c '$clocks_script'" 2> /dev/null || true; } | { tr -d '\r' || true; } | { head -n 1 || true; })"
        if [[ -n "$line" ]]; then
            echo "$line" >> "$out_file"
        else
            echo "$i$blanks" >> "$out_file"
        fi
        (( i + 1 < seconds )) && sleep 1
    done

    echo "wrote $out_file"
    awk -F, 'NR>1 && $2+0>0 { n++; s+=$2; if (m==0 || $2+0<m) m=$2+0 }
        $3+0>0 { b+=$3; bn++ }
        $13+0>0 { c=$13+0; if (t==0 || c>t) t=c }
        $14+0>0 { c=$14+0; if (u==0 || c>u) u=c }
        END {
            if (n==0) { print "no samples with a GPU clock reading"; exit }
            printf "gpuclk: mean %.0f MHz, min %.0f MHz over %d s\n", s/n/1e6, m/1e6, n
            if (bn) printf "gpu busy: mean %.1f percent\n", b/bn
            if (t+0>0) printf "CPU temperature: peak %.1f C (cpu-0 cluster), %.1f C (cpu-1 cluster)\n", t/1000, u/1000
            else print "CPU temperature: no zone read without root"
        }' "$out_file"
}

cmd_perf() {
    local package="$1" label="$2" seconds="${3:-${VITA3K_PERF_SECONDS:-30}}"
    local out_dir="$repo_root/tmp/perf/$label"
    local symfs="/data/local/tmp/native_libs"
    mkdir -p "$out_dir"

    # Find the unstripped library. The Gradle native build puts it under a hash
    # directory, so the newest match wins. find fails when the build directory
    # does not exist yet, and `set -e` would end the script on that inside the
    # substitution, before the message below could explain why.
    local lib="${VITA3K_UNSTRIPPED_LIB:-}"
    if [[ -z "$lib" ]]; then
        # xargs -r is GNU-only and fails on macOS, so the loop is written out.
        # The `|| true` stops a SIGPIPE from find, which is what pipefail reports
        # as a failure once head has already taken its line.
        local newest=""
        while read -r f; do
            if [[ -z "$newest" || "$f" -nt "$newest" ]]; then newest="$f"; fi
        done < <({ find "$repo_root/android/app/build/intermediates/cxx" -path '*arm64-v8a/libVita3K.so' -type f 2> /dev/null || true; })
        lib="$newest"
    fi
    [[ -n "$lib" && -f "$lib" ]] || die "unstripped libVita3K.so not found. Build the APK, or set VITA3K_UNSTRIPPED_LIB."

    adb shell mkdir -p "$symfs"
    adb push "$lib" "$symfs/libVita3K.so" > /dev/null

    echo "recording ${seconds}s into $out_dir"
    # dwarf is the call graph the kernel unwinds without frame pointers, which a
    # release build strips. --symfs points at the unstripped copy so the report
    # resolves names.
    adb shell "simpleperf record -g --call-graph dwarf --symfs $symfs --app $package -o /data/local/tmp/perf.data --duration $seconds" ||
        die "simpleperf failed. Check that <profileable android:shell=\"true\" /> is in the manifest and the app is running."

    adb pull /data/local/tmp/perf.data "$out_dir/perf.data" > /dev/null
    adb pull "$symfs/libVita3K.so" "$out_dir/libVita3K.so" > /dev/null 2>&1 || true
    echo "wrote $out_dir/perf.data"

    # report-sample and binary_cache_builder.py live in the NDK, not on the
    # device. perf.data and the symbols are the raw result and are already
    # pulled, so a missing NDK only costs the HTML view.
    local ndk="${ANDROID_NDK_HOME:-${ANDROID_NDK_ROOT:-}}"
    local report_py cache_builder
    if [[ -n "$ndk" ]]; then
        report_py="$({ find "$ndk" -name report-sample -type f 2> /dev/null || true; } | { head -n 1 || true; })"
        cache_builder="$({ find "$ndk" -name binary_cache_builder.py -type f 2> /dev/null || true; } | { head -n 1 || true; })"
    fi
    if [[ -n "$report_py" && -n "$cache_builder" ]]; then
        ( cd "$out_dir" && python3 "$cache_builder" . perf.data && python3 "$report_py" --symfs . -i perf.data -o report.html ) > "$out_dir/report.txt" 2>&1 || true
        if [[ -f "$out_dir/report.html" ]]; then
            echo "wrote $out_dir/report.html"
        else
            # The `|| true` above is what lets control reach here with no report,
            # so this branch has to say so and return success.
            echo "the NDK report tools ran but produced no report. See $out_dir/report.txt"
        fi
    else
        echo "no NDK at ANDROID_NDK_HOME, so no HTML report. perf.data and the symbols are in $out_dir."
    fi
}

# Perfetto from adb. There is no vulkan or egl atrace category on Android 13, so
# the categories are the ones that exist: sched for the thread states, power for
# the clocks, gfx and view for the frames, thermal for the temperature.
cmd_trace() {
    local package="$1" label="$2" seconds="${3:-30}"
    local out_dir="$repo_root/tmp/trace/$label"
    local remote="/data/misc/perfetto-traces/$label.pftrace"
    mkdir -p "$out_dir"

    echo "recording ${seconds}s into $out_dir"
    adb shell "perfetto --txt -t ${seconds}s -a $package -o $remote \
        ATRACE_CAT=sched ATRACE_CAT=freq ATRACE_CAT=gfx ATRACE_CAT=view \
        ATRACE_CAT=hal ATRACE_CAT=sync ATRACE_CAT=idle ATRACE_CAT=power \
        ATRACE_CAT=thermal ATRACE_CAT=membus \
        sched/sched_switch sched/sched_blocked_reason \
        power/cpu_frequency power/gpu_frequency \
        gpu_mem/gpu_mem_total thermal/thermal_temperature" || die "perfetto failed"

    adb pull "$remote" "$out_dir/trace.pftrace" > /dev/null
    echo "wrote $out_dir/trace.pftrace"
    echo "Open it at https://ui.perfetto.dev and load the file."
}

# The layer name has to match SurfaceFlinger, and it carries a hash that changes
# when the activity is recreated, so it is read from --list and not hard-coded.
cmd_latency() {
    local package="$1" label="$2" seconds="${3:-60}"
    local out_dir="$repo_root/tmp/latency/$label" out_file="$repo_root/tmp/latency/$label/latency.csv"
    mkdir -p "$out_dir"

    local layer
    # The layer name carries a hash that changes when the activity is recreated,
    # so it is read from --list and never hard-coded. A line is the app's own
    # surface when it names the package and is not one of the bookkeeping entries
    # SurfaceFlinger also lists for the same package.
    #
    # `set -e` would end the script on a failing grep inside the substitution,
    # before the message below could explain why, so the pipeline is guarded.
    layer="$({ adb shell dumpsys SurfaceFlinger --list 2>/dev/null || true; } | tr -d '\r' |
        { grep -F "$package" || true; } |
        { grep -v -E 'ActivityRecord|InputSink|WindowToken|ActivityInputSink|^Task=|ImeContainer|StartingWindow|starting window' || true; } |
        { head -n 1 || true; } | sed 's/^ *//;s/ *$//')"
    [[ -n "$layer" ]] || die "no SurfaceFlinger layer for $package. Is the app running?"
    echo "$layer" > "$out_dir/layer.txt"

    echo "second,desired_ns,actual_ns,ready_ns" > "$out_file"
    local i out
    for ((i = 0; i < seconds; i++)); do
        out="$({ adb shell dumpsys SurfaceFlinger --latency "'$layer'" 2> /dev/null || true; } | { tr -d '\r' || true; })"
        # Line 1 is the refresh period. After that come three columns per frame.
        # SurfaceFlinger reports a frame it did not see as 0 or as the unsigned
        # -1, so those rows are dropped rather than counted as instant frames.
        printf '%s\n' "$out" | awk -v s="$i" '
            NR == 1 { next }
            $1 == 0 || $1 == 18446744073709551615 { next }
            NF >= 3 { print s "," $1 "," $2 "," $3 }
        ' >> "$out_file"
        (( i + 1 < seconds )) && sleep 1
    done
    echo "wrote $out_file (layer: $layer)"

    # SurfaceFlinger answers with the refresh period and no frame rows on Android
    # 13 for this app, measured on the Pocket S on 2026-10-03. It does the same for
    # the SurfaceView(BLAST) layer the app draws into, and after --latency-clear,
    # so the interface carries no frame timing here. Say so rather than leave a
    # header-only CSV that reads like a run of zero-length frames.
    #
    # The frame timing is in presents.csv from perf-log, which timestamps every
    # host present, and perf_report.py reports it from there.
    local rows
    rows="$(grep -c . "$out_file" 2>/dev/null || echo 0)"
    if (( rows <= 1 )); then
        echo "no frame rows from SurfaceFlinger for this layer on this Android version."
        echo "Use presents.csv from perf-log instead: device.sh pull-perf, then perf_report.py."
    fi
}

cmd_screenshot() {
    local file="$1" remote="/sdcard/vita3k-screenshot.png"
    mkdir -p "$(dirname "$file")"
    adb shell screencap -p "$remote"
    adb pull "$remote" "$file" > /dev/null
    adb shell rm "$remote"
    echo "$file"
}

# One measurement set for one title: the A/B/A/B/A sequence of the protocol in
# .scratch/pocket-s-android13/spec.md, with the power sampler and the SurfaceFlinger
# latency sampler running beside each run, and the report written per run.
#
# The A and B values are config.yml settings, so A and B use the same APK and the
# only difference between the runs is the setting. That is what the protocol asks
# for, and it is why this takes settings and not two APKs.
baseline_one_run() {
    local package="$1" title_id="$2" label="$3" seconds="$4" warmup="$5"
    shift 5
    local settings=("$@")

    local dir="$repo_root/tmp/baseline/$label"
    mkdir -p "$dir"

    # The settings go in before the run and are left in afterwards, so the caller
    # sees what the run saw. A run is only comparable with the next one if the
    # file says what it says.
    local kv
    for kv in "${settings[@]}"; do
        cmd_config_set "$package" "${kv%%=*}" "${kv#*=}" > /dev/null
    done

    # Reaching the scene takes about two minutes: gameplay_scene.sh waits 50 s for
    # the boot, then 9 s, 4 s and 55 s through the menus. The samplers have to
    # cover the movement and not the walk, so they start after the scene is
    # reached. Starting them first and sampling for `seconds` would record the
    # menus, and run_is_valid.sh rejects exactly that.
    #
    # The scene script runs in the background and is waited for, because the wait
    # is what puts the samplers in the middle of the run. run_is_valid.sh reads the
    # thermal status the sampler recorded during the run; a status read afterwards
    # cannot say whether the run itself was limited.
    if [[ -n "${BASELINE_SCENE:-}" ]]; then
        MOVE_SECONDS="$(( seconds + settle_after_scene ))" "$BASELINE_SCENE" \
            "$package" "$label" "$dir" > "$dir/scene.txt" 2>&1 &
    else
        MOVE_SECONDS="$(( seconds + settle_after_scene ))" \
            "$repo_root/tools/android/gameplay_scene.sh" \
            "$package" "$label" "$dir" > "$dir/scene.txt" 2>&1 &
    fi
    local scene_pid=$!

    sleep "$lead_seconds"
    device_power_sample.sh "$dir/power.csv" "$seconds" > /dev/null 2>&1 &
    local power_pid=$!
    cmd_latency "$package" "$label" "$seconds" > /dev/null 2>&1 &
    local latency_pid=$!

    wait "$power_pid" 2> /dev/null || true
    wait "$latency_pid" 2> /dev/null || true
    wait "$scene_pid" 2> /dev/null || true

    # The gameplay folder holds the perf CSVs; the report wants the power CSV and
    # latency CSV beside them, and the latency sampler wrote to its own folder.
    cp -f "$repo_root/tmp/latency/$label/latency.csv" "$dir/" 2> /dev/null || true
    cp -f "$repo_root/tmp/clocks/$label/clocks.csv" "$dir/" 2> /dev/null || true

    # Validity is decided before the report, and an invalid run does not get one.
    # Criterion 6 of the spec says a run that fails this check is not a
    # measurement, so printing the table anyway would invite quoting a number the
    # protocol rejects. The reasons are printed instead.
    local valid="INVALID"
    "$repo_root/tools/android/run_is_valid.sh" "$dir" "$dir/power.csv" > "$dir/validity.txt" 2>&1 || true
    valid="$(head -n 1 "$dir/validity.txt")"

    echo "$label: $valid"
    if [[ "$valid" == "VALID" ]]; then
        python3 "$repo_root/tools/android/perf_report.py" "$dir" \
            --target "${BASELINE_TARGET:-30}" --warmup "$warmup" > "$dir/report.txt" 2>&1 || true
        sed 's/^/  /' "$dir/report.txt" 2> /dev/null || true
    else
        echo "  no report: this run is not a measurement. See $dir/validity.txt"
        sed 's/^/  /' "$dir/validity.txt" 2> /dev/null || true
        return 0
    fi
}

# The A/B/A/B/A sequence. Five runs, so the spread of the three A runs is known
# and criterion 3 of the spec can be applied: B differs from A only if its
# difference from the A mean is larger than that spread.
cmd_baseline() {
    local package="$1" title_id="$2" label="$3"
    shift 3

    local seconds="${BASELINE_SECONDS:-60}"
    local warmup="${BASELINE_WARMUP:-120}"
    local settle="${BASELINE_SETTLE:-180}"

    # How long the scene walk takes before the movement starts. gameplay_scene.sh
    # waits WAIT_BOOT 50, WAIT_MENU 9, WAIT_DIALOG 4 and WAIT_LEVEL 55 by default,
    # so 118 s. The value is read from the script rather than hard-coded here, so a
    # change to those waits does not silently move the sampling window off the
    # gameplay. A caller with its own scene script sets BASELINE_LEAD.
    local lead_seconds="${BASELINE_LEAD:-}"
    if [[ -z "$lead_seconds" ]]; then
        local scene_script="$repo_root/tools/android/gameplay_scene.sh"
        [[ -n "${BASELINE_SCENE:-}" ]] && scene_script="$BASELINE_SCENE"
        lead_seconds="$(awk '
            # The default is written wait_boot="${WAIT_BOOT:-50}", so the whole
            # assignment is field 1 and the number is what follows the last ":-".
            /^wait_boot=/ { gsub(/.*:-/, "", $1); gsub(/}.*/, "", $1); b = $1 }
            /^wait_menu=/ { gsub(/.*:-/, "", $1); gsub(/}.*/, "", $1); m = $1 }
            /^wait_dialog=/ { gsub(/.*:-/, "", $1); gsub(/}.*/, "", $1); d = $1 }
            /^wait_level=/ { gsub(/.*:-/, "", $1); gsub(/}.*/, "", $1); l = $1 }
            END { print b + m + d + l }
        ' "$scene_script" 2> /dev/null || echo "")"
        [[ "$lead_seconds" =~ ^[0-9]+$ ]] || die "cannot read the scene walk length from $scene_script. Set BASELINE_LEAD to the seconds before the gameplay starts."
    fi

    # The movement has to outlast the sampling window, so the sampler stops while
    # the camera is still moving and not at the end of it.
    local settle_after_scene=15
    local first_temp=""

    # The protocol runs `am kill-all` once before the warm-up and not between runs:
    # between runs it would change the cache state the warm-up exists to fill.
    adb shell am kill-all > /dev/null 2>&1 || true

    echo "baseline: warm-up run, discarded"
    baseline_one_run "$package" "$title_id" "$label-warmup" "$seconds" "$warmup" "$@" > /dev/null 2>&1 || true

    local round name settings=()
    for round in A B A B A; do
        name="$label-$round$round"
        # The A runs carry no setting, so A is the reference. B carries the pairs
        # the caller gave after the label. `if` and not `&&`, because under `set -e`
        # a false left side of `&&` as the last statement of a loop body ends the
        # script.
        settings=()
        if [[ "$round" == "B" ]]; then
            settings=("$@")
        fi

        baseline_one_run "$package" "$title_id" "$name" "$seconds" "$warmup" "${settings[@]+"${settings[@]}"}"

        # Cooldown. The temperature has to come back to where the first run
        # started, or the next run is measured on a hotter device than the last.
        #
        # `waited` and `temp` are declared before the loop, not inside it. `local`
        # inside a loop body runs again on every pass, so a `set -u` shell sees an
        # unset variable on the second pass.
        if [[ -z "$first_temp" ]]; then
            first_temp="$(read_cpu_temp)"
        else
            waited=0
            temp=""
            while (( waited < settle )); do
                temp="$(read_cpu_temp)"
                if [[ -n "$temp" ]] && \
                   (( $(awk -v a="$temp" -v b="$first_temp" 'BEGIN { print (a-b < 0 ? b-a : a-b) }') <= 2 )); then
                    break
                fi
                sleep 10
                waited=$(( waited + 10 ))
            done
            if (( waited < settle )); then
                echo "  cooled to ${temp} mdeg in ${waited}s"
            else
                echo "  still ${temp} mdeg after ${settle}s; the next run starts hotter"
            fi
        fi
    done

    echo
    echo "Read each run's report.txt under tmp/baseline/. The set is valid when the"
    echo "three A averages are within 3% of each other; otherwise repeat it once."
}

# The hottest of the two CPU clusters, in millidegrees. The zones named cpu-0-* and
# cpu-1-* read without root on this device; the others return "Invalid argument".
#
# `sh -c` and the payload in a variable, because `adb shell` with a multi-line
# argument loses the quoting: the device shell saw the inner $(...) and the case
# patterns as separate commands and returned nothing. cmd_clocks reads its paths
# the same way.
read_cpu_temp() {
    local script='t=0
for z in /sys/class/thermal/thermal_zone*; do
    n=$(cat $z/type 2>/dev/null)
    case "$n" in
        cpu-0-*|cpu-1-*)
            v=$(cat $z/temp 2>/dev/null)
            case "$v" in ""|*[!0-9-]*) continue;; esac
            [ "$v" -gt "$t" ] && t=$v ;;
    esac
done
echo $t'
    adb shell "sh -c '$script'" 2> /dev/null | tr -dc '0-9'
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
    hold)
        need_args 2 "$@"
        cmd_hold "$@"
        ;;
    thermal)
        need_args 1 "$@"
        die "the thermal command is now clocks: tools/android/device.sh clocks <package> <label> [secs]"
        ;;
    clocks)
        need_args 2 "$@"
        cmd_clocks "$2" "${3:-60}"
        ;;
    perf)
        need_args 2 "$@"
        cmd_perf "$1" "$2" "${3:-}"
        ;;
    trace)
        need_args 3 "$@"
        cmd_trace "$1" "$2" "$3"
        ;;
    latency)
        need_args 2 "$@"
        cmd_latency "$1" "$2" "${3:-60}"
        ;;
    screenshot)
        need_args 1 "$@"
        cmd_screenshot "$1"
        ;;
    baseline)
        need_args 3 "$@"
        cmd_baseline "$1" "$2" "$3" "${@:4}"
        ;;
    *) usage 1 ;;
esac
