#!/usr/bin/env python3
"""Unit test for perf_report.py. Run: python3 tools/android/test_perf_report.py"""

import os
import shutil
import tempfile
import unittest

import perf_report

HERE = os.path.dirname(os.path.abspath(__file__))
BASIC = os.path.join(HERE, "testdata", "perf_basic")


def write_power(folder, name, rows):
    """Write a device_power_sample.sh CSV with the given rows.

    The header is copied from the real script, so a column rename there breaks
    these tests instead of silently reading the wrong field.
    """
    header = ("second,power_uw,cpu0_khz,cpu3_khz,cpu7_khz,gpuclk_hz,gpu_busy_pct,"
              "max_gpuclk_hz,throttling,temp_gpu_mdeg,temp_cpu1_mdeg,capacity_pct,"
              "thermal_status\n")
    with open(os.path.join(folder, name), "w") as f:
        f.write(header)
        for row in rows:
            f.write(",".join(str(v) for v in row) + "\n")


def write_latency(folder, desired_ns):
    with open(os.path.join(folder, "latency.csv"), "w") as f:
        f.write("second,desired_ns,actual_ns,ready_ns\n")
        for i, ns in enumerate(desired_ns):
            f.write(f"{i},{ns},{ns},0\n")


class BasicTest(unittest.TestCase):
    def test_frames_only(self):
        table = dict(perf_report.build(BASIC, 30, 1))
        self.assertEqual(table["target FPS"], "30")
        self.assertEqual(table["seconds measured"], "3")
        self.assertEqual(table["average FPS"], "29.33")
        self.assertEqual(table["frame interval p99"], "100.00 ms")
        self.assertEqual(table["presents per second"], "60.00")
        self.assertIn("no power CSV", table["device power"])
        self.assertEqual(table["SurfaceFlinger interval p99"], "no latency.csv")

    def test_late_fraction(self):
        table = dict(perf_report.build(BASIC, 30, 1))
        # One interval of 100 ms against a 50 ms limit for a 30 FPS title.
        self.assertIn("late frames (over 50.0 ms)", table)
        self.assertTrue(table["late frames (over 50.0 ms)"].startswith("1 of "))


class PowerTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.folder = self.tmp.name
        for name in ("frames.csv", "presents.csv"):
            shutil.copy(os.path.join(BASIC, name), self.folder)

    def tearDown(self):
        self.tmp.cleanup()

    def test_find_power_csv_by_header(self):
        # clocks.csv and latency.csv have their own headers and must not be
        # picked up, and the name of the power file is the caller's choice.
        write_latency(self.folder, [0, 16666666])
        with open(os.path.join(self.folder, "clocks.csv"), "w") as f:
            f.write("second,gpuclk_hz\n0,1\n")
        self.assertIsNone(perf_report.find_power_csv(self.folder))
        write_power(self.folder, "turnip-run.csv",
                    [(0, 3_000_000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)])
        self.assertTrue(perf_report.find_power_csv(self.folder).endswith("turnip-run.csv"))

    def test_means_and_units(self):
        write_power(self.folder, "power-3s.csv", [
            (0, 3_000_000, 1_000_000, 2_000_000, 595_000, 680_000_000, 90, 680_000_000, 0, 40_000, 41_000, 80, 2),
            (1, 4_000_000, 1_000_000, 2_000_000, 595_000, 680_000_000, 94, 680_000_000, 0, 41_000, 42_000, 80, 2),
            (2, 5_000_000, 1_000_000, 2_000_000, 595_000, 680_000_000, 92, 680_000_000, 0, 42_000, 43_000, 80, 3),
        ])
        table = dict(perf_report.build(self.folder, 30, 1))
        self.assertIn("4.000 W mean", table["device power"])
        self.assertIn("3.000 W min", table["device power"])
        self.assertEqual(table["GPU busy"], "92.0%")
        self.assertIn("680 MHz (cap 680 MHz)", table["GPU clock"])
        self.assertEqual(table["CPU clock prime"], "595 MHz")
        # The worst status during the run, not the last one.
        self.assertEqual(table["thermal status worst"], "3")

    def test_zero_column_is_not_a_reading(self):
        # A column of zeros means the file was unreadable, not a real value.
        write_power(self.folder, "power-2s.csv", [
            (0, 3_000_000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
            (1, 3_000_000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
        ])
        table = dict(perf_report.build(self.folder, 30, 1))
        self.assertEqual(table["GPU busy"], "no reading")
        self.assertEqual(table["thermal status worst"], "not sampled during the run")

    def test_empty_power_csv(self):
        write_power(self.folder, "power-0s.csv", [])
        table = dict(perf_report.build(self.folder, 30, 1))
        # An unreadable power file must not take the table down with it.
        self.assertIn("no power CSV", table["device power"])


class LatencyTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.folder = self.tmp.name
        for name in ("frames.csv", "presents.csv"):
            shutil.copy(os.path.join(BASIC, name), self.folder)

    def tearDown(self):
        self.tmp.cleanup()

    def test_interval_from_desired_column(self):
        # 60 presents 16.667 ms apart, with one gap of 33.3 ms.
        desired = [int(i * 16666666) for i in range(60)]
        desired[30] += 16666666
        write_latency(self.folder, desired)
        table = dict(perf_report.build(self.folder, 30, 1))
        # 60 samples give 59 intervals, so the row count is not the frame count.
        self.assertIn("33.33 ms over 59 frames", table["SurfaceFlinger interval p99"])

    def test_too_few_frames(self):
        write_latency(self.folder, [16666666])
        table = dict(perf_report.build(self.folder, 30, 1))
        self.assertIn("too few for an interval", table["SurfaceFlinger interval p99"])


class DriverTest(unittest.TestCase):
    def test_driver_from_summary(self):
        with tempfile.TemporaryDirectory() as tmp:
            for name in ("frames.csv", "presents.csv"):
                shutil.copy(os.path.join(BASIC, name), tmp)
            with open(os.path.join(tmp, "summary.txt"), "w") as f:
                f.write("package: org.vita3k.emulator\ncustom-driver-name: turnip\n")
            table = dict(perf_report.build(tmp, 30, 1))
            self.assertEqual(table["driver"], "turnip")

    def test_no_summary(self):
        with tempfile.TemporaryDirectory() as tmp:
            for name in ("frames.csv", "presents.csv"):
                shutil.copy(os.path.join(BASIC, name), tmp)
            table = dict(perf_report.build(tmp, 30, 1))
            self.assertEqual(table["driver"], "not in summary.txt")


if __name__ == "__main__":
    unittest.main()