#!/usr/bin/env python3
"""Summarize the perf-log CSV files of one run.

Usage: tools/android/perf_summary.py <folder> [--target 30|60] [--warmup 30]

<folder> holds frames.csv (one line per emulated frame: steady_us,title_id)
and, optionally, presents.csv (one line per host present: steady_us,result).
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
    times = []
    with open(path, newline="") as f:
        reader = csv.reader(f)
        next(reader, None)
        for row in reader:
            if row and row[0].strip():
                times.append(int(row[0]))
    return times


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
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("folder")
    parser.add_argument("--target", type=int, choices=(30, 60), default=60)
    parser.add_argument("--warmup", type=float, default=30)
    args = parser.parse_args(argv)

    try:
        r = summarize(args.folder, args.target, args.warmup)
    except (OSError, ValueError) as e:
        print(f"perf_summary.py: {e}", file=sys.stderr)
        return 1

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
    return 0


if __name__ == "__main__":
    sys.exit(main())
