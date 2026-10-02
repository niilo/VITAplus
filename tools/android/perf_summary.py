#!/usr/bin/env python3
"""Summarize the perf-log CSV files of one run.

Usage: tools/android/perf_summary.py <folder> [--target 30|60] [--warmup 30]

<folder> holds frames.csv (one line per emulated frame: steady_us,title_id)
and, optionally, presents.csv (one line per host present: steady_us,result)
and scenes.csv (one line per scene: steady_us,draws,record_us).
The first --warmup seconds after the first frame are skipped. Only whole
seconds count for the per-second values.
"""

import argparse
import csv
import math
import os
import sys


def read_times(path):
    """Return the first column of a CSV file with a header, as integers."""
    return [int(row[0]) for row in read_rows(path) if row[0]]


def read_rows(path):
    """Return the data rows of a CSV file with a header, as lists of strings."""
    rows = []
    with open(path, newline="") as f:
        reader = csv.reader(f)
        next(reader, None)  # the header
        for row in reader:
            if row and row[0].strip():
                rows.append(row)
    return rows


def percentile(values, pct):
    """Nearest-rank percentile of a non-empty list."""
    ordered = sorted(values)
    rank = max(1, math.ceil(pct / 100 * len(ordered)))
    return ordered[rank - 1]


def summarize(folder, target, warmup):
    frames = read_times(os.path.join(folder, "frames.csv"))
    if len(frames) < 2:
        raise ValueError("frames.csv has fewer than 2 frames")

    start = frames[0] + int(warmup * 1_000_000)
    frames = [t for t in frames if t >= start]
    if len(frames) < 2:
        raise ValueError("fewer than 2 frames after the warm-up")

    seconds = (frames[-1] - start) // 1_000_000
    if seconds < 1:
        raise ValueError("less than one whole second after the warm-up")
    end = start + seconds * 1_000_000

    per_second = [0] * seconds
    for t in frames:
        if t < end:
            per_second[(t - start) // 1_000_000] += 1

    intervals_ms = [(b - a) / 1000 for a, b in zip(frames, frames[1:])]
    limit_ms = 1.5 * 1000 / target

    result = {
        "seconds": seconds,
        "average_fps": sum(per_second) / seconds,
        "percent_at_target": 100 * sum(1 for n in per_second if n >= target - 1) / seconds,
        "p99_interval_ms": percentile(intervals_ms, 99),
        "max_interval_ms": max(intervals_ms),
        "intervals_over_limit": sum(1 for i in intervals_ms if i > limit_ms),
        "limit_ms": limit_ms,
        "presents_per_second": None,
    }

    presents_path = os.path.join(folder, "presents.csv")
    if os.path.exists(presents_path):
        presents = read_times(presents_path)
        result["presents_per_second"] = sum(1 for t in presents if start <= t < end) / seconds

    # scenes.csv holds host times, so it says how long the renderer thread spent
    # recording, not how long the GPU was busy. One row is one submit, not one
    # GXM scene: a scene that the guest splits submits more than once. Draw
    # counts are exact.
    scenes_path = os.path.join(folder, "scenes.csv")
    result["scenes_per_second"] = None
    result["draws_per_scene"] = None
    result["max_draws_per_scene"] = None
    result["record_ms_p99"] = None
    if os.path.exists(scenes_path):
        rows = [r for r in read_rows(scenes_path) if start <= int(r[0]) < end and len(r) >= 3]
        if rows:
            durations = [int(r[2]) / 1000 for r in rows]
            draws = [int(r[1]) for r in rows]
            result["scenes_per_second"] = len(rows) / seconds
            result["draws_per_scene"] = sum(draws) / len(draws)
            result["max_draws_per_scene"] = max(draws)
            result["record_ms_p99"] = percentile(durations, 99)
        else:
            result["scenes_note"] = "scenes.csv has no rows in the measured window"
    return result


def as_csv(r):
    """Return the result as CSV lines, one key and value per line."""
    lines = ["key,value"]
    for key, value in r.items():
        if isinstance(value, float):
            value = f"{value:.4f}"
        elif value is None:
            value = ""
        lines.append(f"{key},{value}")
    return "\n".join(lines) + "\n"


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("folder")
    parser.add_argument("--target", type=int, choices=(30, 60), default=60)
    parser.add_argument("--warmup", type=float, default=30)
    parser.add_argument("--csv", action="store_true",
                        help="print the result as CSV instead of as text")
    args = parser.parse_args(argv)

    try:
        r = summarize(args.folder, args.target, args.warmup)
    except (OSError, ValueError) as e:
        print(f"perf_summary.py: {e}", file=sys.stderr)
        return 1

    if args.csv:
        print(as_csv(r), end="")
        return 0

    print(f"seconds measured:        {r['seconds']}")
    print(f"average FPS:             {r['average_fps']:.2f}")
    print(f"seconds at target:       {r['percent_at_target']:.1f}% (fps >= {args.target - 1})")
    print(f"frame interval p99:      {r['p99_interval_ms']:.2f} ms")
    print(f"frame interval max:      {r['max_interval_ms']:.2f} ms")
    print(f"intervals over {r['limit_ms']:.1f} ms: {r['intervals_over_limit']}")
    if r["presents_per_second"] is None:
        print("presents per second:     no presents.csv")
    else:
        print(f"presents per second:     {r['presents_per_second']:.2f}")
    if r["scenes_per_second"] is None:
        note = r.get("scenes_note", "no scenes.csv")
        print(f"scenes per second:       {note}")
    else:
        print(f"scenes per second:       {r['scenes_per_second']:.2f}")
        print(f"draws per scene:         {r['draws_per_scene']:.2f} (max {r['max_draws_per_scene']})")
        print(f"scene record p99:        {r['record_ms_p99']:.2f} ms (host time)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
