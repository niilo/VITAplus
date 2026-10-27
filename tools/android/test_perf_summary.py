#!/usr/bin/env python3
"""Unit test for perf_summary.py. Run: python3 tools/android/test_perf_summary.py"""

import os
import shutil
import tempfile
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

    # scenes.csv has 30 scenes in the 3 measured seconds, one every 3 frames,
    # each with 2 draws and 8 ms of host record time.
    def test_scenes(self):
        r = perf_summary.summarize(os.path.join(HERE, "testdata", "perf_basic"), target=30, warmup=1)
        self.assertAlmostEqual(r["scenes_per_second"], 10.0)
        self.assertAlmostEqual(r["draws_per_scene"], 2.0)
        self.assertEqual(r["max_draws_per_scene"], 2)
        self.assertAlmostEqual(r["record_ms_p99"], 8.0)

    # A folder without scenes.csv reports no scene values instead of failing.
    def test_no_scenes_file(self):
        with tempfile.TemporaryDirectory() as tmp:
            for name in ("frames.csv", "presents.csv"):
                shutil.copy(os.path.join(HERE, "testdata", "perf_basic", name), tmp)
            r = perf_summary.summarize(tmp, target=30, warmup=1)
            self.assertIsNone(r["scenes_per_second"])
            self.assertIsNone(r["draws_per_scene"])
            self.assertIsNone(r["max_draws_per_scene"])
            self.assertIsNone(r["record_ms_p99"])

    # scenes.csv with rows only outside the measured window reports that,
    # not "no scenes.csv", and leaves every scene value unset.
    def test_scenes_outside_window(self):
        with tempfile.TemporaryDirectory() as tmp:
            for name in ("frames.csv", "presents.csv"):
                shutil.copy(os.path.join(HERE, "testdata", "perf_basic", name), tmp)
            with open(os.path.join(tmp, "scenes.csv"), "w") as f:
                f.write("steady_us,draws,record_us\n1,2,8000\n")
            r = perf_summary.summarize(tmp, target=30, warmup=1)
            self.assertIsNone(r["scenes_per_second"])
            self.assertEqual(r["scenes_note"], "scenes.csv has no rows in the measured window")

    def test_csv_output(self):
        out = perf_summary.as_csv(perf_summary.summarize(
            os.path.join(HERE, "testdata", "perf_basic"), target=30, warmup=1))
        lines = out.strip().splitlines()
        self.assertEqual(lines[0], "key,value")
        self.assertIn("average_fps,29.3333", lines)
        self.assertIn("draws_per_scene,2.0000", lines)


if __name__ == "__main__":
    unittest.main()
