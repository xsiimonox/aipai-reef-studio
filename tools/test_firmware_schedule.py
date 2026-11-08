"""Regression checks for the 24-hour curves persisted inside AIPAI fixtures."""
import ast
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
source = ROOT / "custom_components/aipai_light/studio.py"
tree = ast.parse(source.read_text())
names = {"levels_at", "fit_levels", "firmware_schedule_rows", "needs_startup_hold"}
definitions = [
    node for node in tree.body
    if isinstance(node, ast.FunctionDef) and node.name in names
]
namespace = {"DAY": 1440}
exec(compile(ast.Module(body=definitions, type_ignores=[]), str(source), "exec"), namespace)
firmware_schedule_rows = namespace["firmware_schedule_rows"]
needs_startup_hold = namespace["needs_startup_hold"]


class FirmwareScheduleTests(unittest.TestCase):
    def test_only_confirmed_device_program_is_safe_to_resume_read_only(self):
        self.assertFalse(needs_startup_hold({
            "program": "day", "program_execution": "device",
            "scene": None, "preview": None, "restore": None,
        }))
        self.assertTrue(needs_startup_hold({"program": "day"}))
        self.assertTrue(needs_startup_hold({
            "program": "day", "program_execution": "device", "scene": {"id": "photo"},
        }))

    def test_samples_every_full_hour_and_keeps_six_channel_layout(self):
        points = [
            {"minute": 0, "levels": [0] * 6},
            {"minute": 600, "levels": [100, 80, 60, 40, 20, 10]},
            {"minute": 1200, "levels": [0] * 6},
        ]
        rows = firmware_schedule_rows(points, 6, 6)
        self.assertEqual(len(rows), 6)
        white = [int(value) for value in rows[0].split(",")]
        self.assertEqual(len(white), 24)
        self.assertEqual(white[5], 50)
        self.assertEqual(white[10], 100)
        self.assertEqual(white[15], 50)

    def test_hidden_firmware_channels_are_zero(self):
        points = [
            {"minute": 0, "levels": [10, 20, 30, 40, 50, 60]},
            {"minute": 720, "levels": [20, 30, 40, 50, 60, 70]},
        ]
        rows = firmware_schedule_rows(points, 6, 8)
        self.assertEqual(len(rows), 8)
        self.assertEqual(set(rows[6].split(",")), {"0"})
        self.assertEqual(set(rows[7].split(",")), {"0"})


if __name__ == "__main__":
    unittest.main()
