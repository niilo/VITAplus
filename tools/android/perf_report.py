#!/usr/bin/env python3
"""Print the measurement table for one run.

Usage: tools/android/perf_report.py <run dir> [--target 30|60] [--warmup 120]

<run dir> is one gameplay folder, as tools/android/gameplay_scene.sh writes it,
plus the power CSV that tools/android/device_power_sample.sh produced beside it.
The table is what every ticket in .scratch/pocket-s-android13/ pastes into its
Answer, so it holds one number per line in a fixed order and names the input
each line came from.

Inputs, all optional except frames.csv:

  frames.csv, presents.csv, scenes.csv   the perf-log files, read by perf_summary
  power-*.csv                             device_power_sample.sh, same folder
  latency.csv                             device.sh latency, same folder
  summary.txt                             gameplay_scene.sh, carries the driver name
  vita3k.log                              read for the build banner

GPU busy percentage is the decisive column. Ticket 00 measured 93 to 99 percent
with the GPU the limit, so a change that moves the busy percentage moves the
limit, and one that does not move it will not move the frame rate.

Power is the whole device, including the display and the Android system. Only
the difference between two runs belongs to the emulator, so this script prints
one run's mean and never a share of it.
"""

import argparse
import csv
import os
import sys

import perf_summary


def find_power_csv(folder):
    """Return the device_power_sample.sh CSV in folder, or None.

    The file is found by its header, not by its name. The sampler takes the
    output path as an argument, so the name is whatever the caller chose, and a
    recorded run keeps it next to the frames. clocks.csv and latency.csv have
    their own headers and are skipped this way too.
    """
    for name in sorted(os.listdir(folder)):
        if not name.endswith(".csv"):
            continue
        path = os.path.join(folder, name)
        try:
            with open(path, newline="") as f:
                header = next(csv.reader(f), [])
        except OSError:
            continue
        if "power_uw" in header:
            return path
    return None


def read_power(path):
    """Return the mean and worst of the power, clock and thermal columns.

    A column with no reading in any row stays None, so the caller can say "no
    reading" instead of printing a zero that looks like a measurement.
    """
    with open(path, newline="") as f:
        rows = list(csv.DictReader(f))
    if not rows:
        raise ValueError("the power CSV has no rows")

    def values_of(column):
        return [float(r[column]) for r in rows
                if r.get(column) not in (None, "") and float(r[column]) > 0]

    def mean_of(column, scale=1.0):
        values = values_of(column)
        return sum(values) / len(values) / scale if values else None

    def worst_of(column):
        values = values_of(column)
        return max(values) if values else None

    power_values = values_of("power_uw")
    return {
        "seconds": len(rows),
        "watts": mean_of("power_uw", 1e6),
        "watts_min": min(power_values) / 1e6 if power_values else None,
        "gpu_busy_pct": mean_of("gpu_busy_pct"),
        "gpuclk_mhz": mean_of("gpuclk_hz", 1e6),
        "max_gpuclk_mhz": mean_of("max_gpuclk_hz", 1e6),
        "cpu0_mhz": mean_of("cpu0_khz", 1e3),
        "cpu3_mhz": mean_of("cpu3_khz", 1e3),
        "cpu7_mhz": mean_of("cpu7_khz", 1e3),
        "thermal_worst": worst_of("thermal_status"),
    }
def read_latency(path):
    """Return the frame interval statistics from the SurfaceFlinger CSV.

    Columns are second, desired_ns, actual_ns, ready_ns. The refresh period is
    the gap between consecutive desired values, so the intervals come from that
    column and not from the second index: the sampler runs at about 1 Hz, so the
    index is not a frame clock.
    """
    with open(path, newline="") as f:
        desired = [int(r["desired_ns"]) for r in csv.DictReader(f)
                   if r.get("desired_ns") not in (None, "") and int(r["desired_ns"]) > 0]
    if len(desired) < 2:
        return {"frames": len(desired)}
    intervals_ms = [(b - a) / 1e6 for a, b in zip(desired, desired[1:])]
    ordered = sorted(intervals_ms)
    rank = max(1, -(-99 * len(ordered) // 100))  # ceil(0.99 * n) without math
    return {
        "frames": len(desired),
        "interval_ms_p99": ordered[rank - 1],
        "interval_ms_max": max(intervals_ms),
    }


def read_driver(folder):
    """Return the driver name gameplay_scene.sh recorded, or None."""
    path = os.path.join(folder, "summary.txt")
    if not os.path.exists(path):
        return None
    with open(path, errors="replace") as f:
        for line in f:
            if line.startswith("custom-driver-name:"):
                return line.split(":", 1)[1].strip() or None
    return None


def watts_note(power):
    if power["watts"] is None:
        return "no reading"
    if power["watts_min"] is None:
        return f"{power['watts']:.3f} W"
    return (f"{power['watts']:.3f} W mean, {power['watts_min']:.3f} W min, "
            f"over {power['seconds']} s")


def mhz_note(value, cap=None):
    if value is None:
        return "no reading"
    if cap:
        return f"{value:.0f} MHz (cap {cap:.0f} MHz)"
    return f"{value:.0f} MHz"


def build(folder, target, warmup):
    """Return the table as a list of (label, value) pairs, in order."""
    frames = perf_summary.summarize(folder, target, warmup)
    rows = []

    driver = read_driver(folder)
    rows.append(("driver", driver if driver else "not in summary.txt"))
    rows.append(("target FPS", str(target)))
    rows.append(("seconds measured", str(frames["seconds"])))
    rows.append(("average FPS", f"{frames['average_fps']:.2f}"))
    rows.append(("frame interval p99", f"{frames['p99_interval_ms']:.2f} ms"))
    rows.append(("frame interval max", f"{frames['max_interval_ms']:.2f} ms"))

    # Criterion 2 wants the fraction of late frames, not only the p99, because
    # on a 60 Hz panel one missed vsync shows up as a 50 ms interval on a 30 FPS
    # title.
    limit_ms = frames["limit_ms"]
    intervals = max(1, round(frames["average_fps"] * frames["seconds"]))
    late = frames["intervals_over_limit"]
    rows.append((f"late frames (over {limit_ms:.1f} ms)",
                 f"{late} of {intervals} ({100 * late / intervals:.2f}%)"))

    if frames["presents_per_second"] is None:
        rows.append(("presents per second", "no presents.csv"))
    else:
        rows.append(("presents per second", f"{frames['presents_per_second']:.2f}"))

    power = None
    power_csv = find_power_csv(folder)
    if power_csv:
        try:
            power = read_power(power_csv)
        except (OSError, ValueError):
            power = None
    if power:
        rows.append(("device power", watts_note(power)))
        rows.append(("GPU busy",
                     f"{power['gpu_busy_pct']:.1f}%" if power["gpu_busy_pct"] is not None
                     else "no reading"))
        rows.append(("GPU clock", mhz_note(power["gpuclk_mhz"], power["max_gpuclk_mhz"])))
        rows.append(("CPU clock small", mhz_note(power["cpu0_mhz"])))
        rows.append(("CPU clock big", mhz_note(power["cpu3_mhz"])))
        rows.append(("CPU clock prime", mhz_note(power["cpu7_mhz"])))
        rows.append(("thermal status worst",
                     f"{power['thermal_worst']:.0f}" if power["thermal_worst"] is not None
                     else "not sampled during the run"))
    else:
        rows.append(("device power",
                     "no power CSV: run device_power_sample.sh beside the run"))

    latency_csv = os.path.join(folder, "latency.csv")
    if not os.path.exists(latency_csv):
        rows.append(("SurfaceFlinger interval p99", "no latency.csv"))
    else:
        try:
            lat = read_latency(latency_csv)
            if "interval_ms_p99" in lat:
                rows.append(("SurfaceFlinger interval p99",
                             f"{lat['interval_ms_p99']:.2f} ms over {lat['frames']} frames"))
            else:
                rows.append(("SurfaceFlinger interval p99",
                             f"{lat['frames']} frames, too few for an interval"))
        except (OSError, ValueError):
            rows.append(("SurfaceFlinger interval p99", "latency.csv could not be read"))
    return rows


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("folder")
    parser.add_argument("--target", type=int, choices=(30, 60), default=30)
    parser.add_argument("--warmup", type=float, default=120)
    args = parser.parse_args(argv)

    try:
        rows = build(args.folder, args.target, args.warmup)
    except (OSError, ValueError) as e:
        print(f"perf_report.py: {e}", file=sys.stderr)
        return 1

    width = max(len(label) for label, _ in rows)
    for label, value in rows:
        print(f"{label.ljust(width)}  {value}")
    return 0


if __name__ == "__main__":
    sys.exit(main())