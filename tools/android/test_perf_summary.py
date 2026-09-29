#!/usr/bin/env python3
"""Unit test for perf_summary.py. Run: python3 tools/android/test_perf_summary.py"""

import os
import unittest

import perf_summary

HERE = os.path.dirname(os.path.abspath(__file__))


class SummaryTest(unittest.TestCase):
    # testdata/perf_basic: 10 frames of warm-up in the first second, then
    # 3 whole seconds with 30, 28 and 30 frames. The frame interval is
    # 33.333 ms, except one gap of 100 ms in the second second. The last
    # frame starts a fourth second, which does not count. presents.csv has
    # 180 presents in the 3 seconds.
    def test_basic(self):
        r = perf_summary.summarize(os.path.join(HERE, "testdata", "perf_basic"), target=30, warmup=1)
        self.assertEqual(r["seconds"], 3)
        self.assertAlmostEqual(r["average_fps"], 88 / 3)
        self.assertAlmostEqual(r["percent_at_target"], 200 / 3)
        self.assertAlmostEqual(r["max_interval_ms"], 100.0)
        self.assertAlmostEqual(r["p99_interval_ms"], 100.0)
        self.assertEqual(r["intervals_over_limit"], 1)
        self.assertAlmostEqual(r["presents_per_second"], 60.0)

    def test_warmup_too_long(self):
        with self.assertRaises(ValueError):
            perf_summary.summarize(os.path.join(HERE, "testdata", "perf_basic"), target=30, warmup=10)

    def test_target_60(self):
        r = perf_summary.summarize(os.path.join(HERE, "testdata", "perf_basic"), target=60, warmup=1)
        self.assertEqual(r["percent_at_target"], 0)
        # Every interval is longer than 25 ms.
        self.assertEqual(r["intervals_over_limit"], 88)


if __name__ == "__main__":
    unittest.main()
