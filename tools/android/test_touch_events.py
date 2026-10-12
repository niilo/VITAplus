#!/usr/bin/env python3
"""Unit test for touch_events.py.

Run: python3 tools/android/test_touch_events.py
"""

import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import touch_events

# Two taps: the first is held for 200 ms at (700, 1200), the second for 50 ms
# at (30, 40). The timestamps come from `getevent -t`, in seconds.
TWO_TAPS = """\
[    10.000000] 0003 002f 00000000
[    10.000000] 0003 0039 00000078
[    10.000000] 0003 0035 000002bc
[    10.000000] 0003 0036 000004b0
[    10.000000] 0000 0000 00000000
[    10.200000] 0003 0039 ffffffff
[    10.200000] 0000 0000 00000000
[    11.000000] 0003 002f 00000000
[    11.000000] 0003 0039 00000079
[    11.000000] 0003 0035 0000001e
[    11.000000] 0003 0036 00000028
[    11.000000] 0000 0000 00000000
[    11.050000] 0003 0039 ffffffff
[    11.050000] 0000 0000 00000000
"""


class ParseTest(unittest.TestCase):
    def test_parses_every_event(self):
        events = touch_events.parse_events(TWO_TAPS)
        self.assertEqual(len(events), 14)

    def test_reads_hex_values(self):
        events = touch_events.parse_events(TWO_TAPS)
        positions = [e for e in events if e[2] == touch_events.ABS_MT_POSITION_X]
        self.assertEqual([e[3] for e in positions], [700, 30])

    def test_reads_a_negative_tracking_id(self):
        events = touch_events.parse_events(TWO_TAPS)
        ids = [e[3] for e in events if e[2] == touch_events.ABS_MT_TRACKING_ID]
        self.assertEqual(ids, [120, -1, 121, -1])

    def test_turns_unsigned_hex_back_into_a_negative(self):
        # getevent writes a release as ffffffff. Read as it stands that is
        # 4294967295, and the release would never be seen.
        self.assertEqual(touch_events.signed_32(0xFFFFFFFF), -1)
        self.assertEqual(touch_events.signed_32(0xFFFFFF9E), -98)
        self.assertEqual(touch_events.signed_32(0x00000078), 120)

    def test_seconds_combine_the_two_parts(self):
        events = touch_events.parse_events(TWO_TAPS)
        self.assertAlmostEqual(events[0][0], 10.0)
        self.assertAlmostEqual(events[0 + 5][0], 10.2)

    def test_accepts_a_device_path_in_the_line(self):
        line = "[     1.000000] /dev/input/event4: 0003 0035 00000064\n"
        events = touch_events.parse_events(line)
        self.assertEqual(events, [(1.0, touch_events.EV_ABS, touch_events.ABS_MT_POSITION_X, 100)])

    def test_skips_lines_it_cannot_read(self):
        text = "add device 1: /dev/input/event4\n\nnot an event\n" + TWO_TAPS
        self.assertEqual(len(touch_events.parse_events(text)), 14)

    def test_drops_abs_codes_that_are_not_touch(self):
        # ABS_MT_TOUCH_MINOR (0x31) is not in the list, so it is left out.
        line = "[     1.000000] 0003 0031 00000005\n"
        self.assertEqual(touch_events.parse_events(line), [])


class GroupTest(unittest.TestCase):
    def test_same_timestamp_lands_in_one_group(self):
        events = touch_events.parse_events(TWO_TAPS)
        groups = touch_events.group_events(events)
        # Four distinct timestamps: the two touches and the two releases.
        self.assertEqual(len(groups), 4)
        self.assertEqual([len(g) for _, g in groups], [5, 2, 5, 2])
        self.assertEqual(sum(len(g) for _, g in groups), len(events))

    def test_a_new_timestamp_starts_a_group(self):
        groups = touch_events.group_events([(1.0, 3, 57, 0), (1.5, 3, 57, 0)])
        self.assertEqual(len(groups), 2)


class TouchTest(unittest.TestCase):
    def test_finds_both_touches(self):
        found = touch_events.touches(touch_events.parse_events(TWO_TAPS))
        self.assertEqual(len(found), 2)
        self.assertEqual((found[0][2], found[0][3]), (700, 1200))
        self.assertEqual((found[1][2], found[1][3]), (30, 40))

    def test_reports_the_hold(self):
        found = touch_events.touches(touch_events.parse_events(TWO_TAPS))
        self.assertAlmostEqual(found[0][1] - found[0][0], 0.2)
        self.assertAlmostEqual(found[1][1] - found[1][0], 0.05)

    def test_a_touch_that_never_ends_is_reported(self):
        text = "[     1.000000] 0003 0039 00000001\n[     1.000000] 0003 0035 00000010\n"
        found = touch_events.touches(touch_events.parse_events(text))
        self.assertEqual(len(found), 1)
        self.assertIsNone(found[0][1])


class ScriptTest(unittest.TestCase):
    def write(self, groups, speed=1.0):
        if speed != 1.0:
            first = groups[0][0]
            groups = [(first + (s - first) / speed, g) for s, g in groups]
        path = os.path.join(tempfile.mkdtemp(), "replay.sh")
        touch_events.write_script(groups, path, "/dev/input/event4", "test")
        with open(path) as handle:
            return handle.read()

    def test_every_event_becomes_a_sendevent_line(self):
        groups = touch_events.group_events(touch_events.parse_events(TWO_TAPS))
        text = self.write(groups)
        self.assertEqual(text.count("sendevent $D "), 14)

    def test_the_gap_between_touches_is_slept(self):
        groups = touch_events.group_events(touch_events.parse_events(TWO_TAPS))
        text = self.write(groups)
        self.assertIn("sleep 0.2000", text)
        self.assertIn("sleep 0.8000", text)

    def test_a_speed_of_two_halves_every_gap(self):
        groups = touch_events.group_events(touch_events.parse_events(TWO_TAPS))
        text = self.write(groups, speed=2.0)
        self.assertIn("sleep 0.1000", text)
        self.assertIn("sleep 0.4000", text)

    def test_the_node_is_in_the_header(self):
        groups = touch_events.group_events(touch_events.parse_events(TWO_TAPS))
        text = self.write(groups)
        self.assertIn("D=/dev/input/event4", text)

    def test_values_are_sent_as_decimals(self):
        groups = [(1.0, [(3, 57, 0), (3, 53, 416)])]
        text = self.write(groups)
        self.assertIn("sendevent $D 3 53 416", text)

    def test_no_sleep_before_the_first_event(self):
        groups = [(10.0, [(3, 57, 0)])]
        header, _, body = self.write(groups).partition("sendevent")
        self.assertNotIn("sleep", header.split("# Source:")[-1])


if __name__ == "__main__":
    unittest.main()
