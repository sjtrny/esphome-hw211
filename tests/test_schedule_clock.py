"""Exercise the production timer translation and persistent transaction model."""

from pathlib import Path
import subprocess
import tempfile
import unittest

import esphome.config_validation as cv

from components import hw211


class ScheduleClockTest(unittest.TestCase):
    def test_translation_dst_and_restart_transactions(self):
        root = Path(__file__).resolve().parents[1]
        with tempfile.TemporaryDirectory(prefix="hw211-clock-test-") as directory:
            binary = Path(directory) / "clock-test"
            subprocess.run(
                ["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                 "-I", str(root / "components/hw211"),
                 str(root / "tests/schedule_clock_test.cpp"), "-o", str(binary)],
                check=True, capture_output=True, text=True,
            )
            subprocess.run([str(binary)], check=True)

    def test_configuration_requires_signed_hours_and_minutes(self):
        for value, minutes in (("+09:00", 540), ("+09:30", 570), ("-03:30", -210),
                               ("-00:30", -30), ("+05:45", 345), ("+14:00", 840)):
            with self.subTest(value=value):
                self.assertEqual(hw211._utc_offset_minutes(value), minutes)
        for value in (9, "9", "9:00", "+09:60", "+14:01", "-14:01", "GMT+09:00"):
            with self.subTest(value=value), self.assertRaises(cv.Invalid):
                hw211._utc_offset_minutes(value)
        with self.assertRaises(cv.Invalid):
            hw211._validate_schedule_clock({hw211.CONF_CREATE_SCHEDULE: False, hw211.CONF_SCHEDULE_CLOCK: {}})


if __name__ == "__main__":
    unittest.main()
