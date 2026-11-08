"""Regression checks for production grouping without installing Home Assistant."""
import ast
from datetime import datetime, timezone
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import AsyncMock
import unittest

ROOT = Path(__file__).resolve().parents[1]
source = ROOT / "custom_components/aipai_light/groups.py"
tree = ast.parse(source.read_text())
definition = next(node for node in tree.body if isinstance(node, ast.ClassDef) and node.name == "ReefGroups")
namespace = {"Store": lambda *args: None, "DOMAIN": "aipai_light"}
exec(compile(ast.Module(body=[definition], type_ignores=[]), str(source), "exec"), namespace)
ReefGroups = namespace["ReefGroups"]
LABELS = ["White", "Blue", "Red", "Green", "Royal Blue", "Purple", "UV", "Warm White"]


class MixedGroupTests(unittest.IsolatedAsyncioTestCase):
    def setup_group(self, counts):
        studios = {}
        for index, count in enumerate(counts):
            serial = str(index)
            hub = SimpleNamespace(serial=serial, labels=LABELS[:count], roads=count, local=True,
                                  available=True, display_name=serial, timezone="Europe/Berlin",
                                  _now=lambda: datetime.now(timezone.utc))
            studio = SimpleNamespace(hub=hub, command=AsyncMock(), library=SimpleNamespace(data={"programs": {}, "scenes": {}}),
                                     data={"program": None, "program_execution": None, "scene": None, "preview": None},
                                     startup_hold=False)
            hub.studio = studio
            studios[serial] = studio
        group = ReefGroups(SimpleNamespace(data={"aipai_light": {key: value.hub for key, value in studios.items()}}))
        group.groups = {"reef": {"name": "Reef", "serials": list(studios)}}
        return group, studios

    async def test_mixed_group_exposes_eight_channels_in_both_orders(self):
        for counts in ([6, 8], [8, 6], [6, 6, 8]):
            group, studios = self.setup_group(counts)
            self.assertEqual(group.snapshot(studios)[0]["labels"], LABELS)

    async def test_six_channel_group_stays_six(self):
        group, studios = self.setup_group([6, 6])
        self.assertEqual(group.snapshot(studios)[0]["labels"], LABELS[:6])

    async def test_preview_routes_only_supported_values_and_keeps_timer(self):
        group, studios = self.setup_group([6, 8])
        levels = [10, 20, 30, 40, 50, 60, 70, 80]
        await group.command("reef", {"action": "preview", "levels": levels, "preview_seconds": 300})
        for studio in studios.values():
            command = studio.command.call_args.args[0]
            self.assertEqual(command["levels"], levels[:studio.hub.roads])
            self.assertEqual(command["preview_seconds"], 300)
            studio.data["preview"] = {"levels": command["levels"], "until": 1234567890}
        self.assertEqual(levels, [10, 20, 30, 40, 50, 60, 70, 80])
        self.assertIsNotNone(group.snapshot(studios)[0]["preview"])

    async def test_program_and_scene_activation_keep_shared_profile_id(self):
        group, studios = self.setup_group([6, 8])
        for kind in ("programs", "scenes"):
            await group.command("reef", {"action": "activate", "kind": kind, "item_id": "eight-channel-profile", "duration": 12})
            for studio in studios.values():
                self.assertEqual(studio.command.call_args.args[0]["item_id"], "eight-channel-profile")


if __name__ == "__main__":
    unittest.main()
