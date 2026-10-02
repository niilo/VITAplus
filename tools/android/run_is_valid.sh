#!/usr/bin/env bash
# Decide whether a measured run may be used as a number.
#
# Usage: tools/android/run_is_valid.sh <gameplay dir> [power csv]
#
# A run is rejected when the emulator stalled, when the device was thermally
# limited, or when the frame data is too thin to say anything. This exists
# because a first attempt at an energy baseline produced 6.94 FPS with a
# 141-second stall and a device at thermal status 3, and quoting it would have
# been wrong.
#
# Prints VALID, or INVALID with one reason per line. Exit status is 0 for VALID
# and 1 for INVALID, so a measurement loop can gate on it.
set -uo pipefail

dir="${1:?usage: run_is_valid.sh <gameplay dir> [power csv]}"
power_csv="${2:-}"

problems=()

# 1. The emulator's own watchdogs. A hang dump or a flip stall means the game
#    stopped presenting, so the frame data describes a hang, not play.
#    vita3k.log is append-only across sessions, so only the last session counts.
#    A session starts at the "Vita3K session start" banner.
if [[ -f "$dir/vita3k.log" ]]; then
    session="$(awk '/Vita3K session start/ { keep = 1; buf = "" } keep { buf = buf $0 "\n" } END { printf "%s", buf }' "$dir/vita3k.log")"
    if [[ -z "$session" ]]; then
        session="$(cat "$dir/vita3k.log")"
    fi
    stalls="$(grep -c 'HANG WATCHDOG\|HANG DUMP\|STUCK-SCENE WATCHDOG' <<< "$session" 2>/dev/null || true)"
    [[ "${stalls:-0}" -gt 0 ]] && problems+=("emulator watchdog fired ${stalls} time(s) in this session: the game stalled")
fi

# 2. Thermal, sampled during the run. Criterion 3 of the spec wants status 3
#    or below. Reading the status after the run cannot tell whether the run
#    itself was limited, so the power CSV carries the status it saw.
status=""
if [[ -n "$power_csv" && -f "$power_csv" ]]; then
    status="$(awk -F, 'NR>1 && $13+0>w { w=$13+0 } END { print w+0 }' "$power_csv")"
fi
if [[ -z "$status" || "$status" == "0" ]]; then
    status="$(adb shell dumpsys thermalservice 2>/dev/null | grep -m1 'Thermal Status' | tr -dc '0-9')"
    [[ -n "$status" && "$status" != "0" ]] && problems+=("no during-run thermal status in the power CSV; read it now instead")
fi
if [[ -n "$status" && "$status" -ge 4 ]]; then
    problems+=("thermal status ${status} during the run, at or past CRITICAL")
elif [[ -n "$status" && "$status" -eq 3 ]]; then
    problems+=("thermal status 3 (SEVERE) during the run: record it, and prefer a cool-device repeat")
fi

# 3. The frame data has to describe play. Fewer than a few hundred frames, or a
#    frame interval in the seconds, means it does not.
if [[ -f "$dir/frames.csv" ]]; then
    frames="$(grep -c . "$dir/frames.csv")"
    (( frames < 300 )) && problems+=("only ${frames} frame rows, too few to characterise")
fi

# 4. The power samples have to cover the window and must not be mostly idle. A
#    mean far below the maximum with a near-zero minimum means the sampler was
#    running while the game was at a menu.
if [[ -n "$power_csv" && -f "$power_csv" ]]; then
    read -r n mean min <<< "$(awk -F, 'NR>1 && $2+0>0 {n++; s+=$2; if (m==0 || $2+0<m) m=$2+0} END {printf "%d %.0f %.0f", n, (n?s/n:0), m}' "$power_csv")"
    if [[ "${n:-0}" -lt 10 ]]; then
        problems+=("only ${n} power samples with a reading")
    elif [[ "${min:-0}" -lt 10000 ]]; then
        problems+=("power minimum ${min} uW is near zero: the sampler covered a menu or a load, not play")
    fi
fi

if (( ${#problems[@]} == 0 )); then
    echo "VALID"
    exit 0
fi

echo "INVALID"
for p in "${problems[@]}"; do
    echo "  - $p"
done
exit 1