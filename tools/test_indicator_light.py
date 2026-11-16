"""Test the production PilotLamp path without Home Assistant or aquarium writes."""
import ast
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import AsyncMock, Mock
import unittest

ROOT = Path(__file__).resolve().parents[1]
source = ROOT / "custom_components/aipai_light/hub.py"
tree = ast.parse(source.read_text())
cls = next(n for n in tree.body if isinstance(n, ast.ClassDef) and n.name == "AipaiLightHub")
method = next(n for n in cls.body if isinstance(n, ast.AsyncFunctionDef) and n.name == "async_set_indicator_light")
namespace = {}
exec(compile(ast.Module(body=[method], type_ignores=[]), str(source), "exec"), namespace)


class IndicatorTests(unittest.IsolatedAsyncioTestCase):
    def hub(self, fail=False):
        commands = []
        async def exchange(prepare):
            commands.extend(await prepare("verified config"))
            if fail:
                raise OSError("No reply")
        return SimpleNamespace(local=True, available=True, indicator_light=None,
                               client=SimpleNamespace(async_studio_exchange=exchange),
                               log_activity=Mock(), _notify=Mock()), commands

    async def test_on_off_use_only_dedicated_command(self):
        hub, commands = self.hub()
        for enabled in (False, True):
            await namespace["async_set_indicator_light"](hub, enabled)
            self.assertIs(hub.indicator_light, enabled)
        self.assertEqual(commands, ["mculed=off", "mculed=on"])

    async def test_failed_command_does_not_claim_success(self):
        hub, _ = self.hub(fail=True)
        with self.assertRaises(OSError):
            await namespace["async_set_indicator_light"](hub, False)
        self.assertIsNone(hub.indicator_light)

    async def test_offline_or_invalid_requests_do_not_write(self):
        hub, commands = self.hub()
        for enabled in ("false", None, 1):
            with self.assertRaises(ValueError):
                await namespace["async_set_indicator_light"](hub, enabled)
        hub.available = False
        with self.assertRaises(OSError):
            await namespace["async_set_indicator_light"](hub, False)
        self.assertEqual(commands, [])


if __name__ == "__main__":
    unittest.main()
