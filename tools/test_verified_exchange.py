"""Exercise production exchange with a simulated lamp; never send network writes."""
import ast
import asyncio
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import AsyncMock
import unittest

ROOT = Path(__file__).resolve().parents[1]
source = ROOT / "custom_components/aipai_light/local_client.py"
tree = ast.parse(source.read_text())
cls = next(n for n in tree.body if isinstance(n, ast.ClassDef))
method = next(n for n in cls.body if isinstance(n, ast.AsyncFunctionDef) and n.name == "async_studio_exchange")


class VerifiedExchangeTests(unittest.IsolatedAsyncioTestCase):
    async def exchange(self, *, mismatch=False, clock="12,00", serial="demo", mode="1"):
        expected = "onx1x35x30x80x50x0x23x1"
        state = SimpleNamespace(serial=serial, mode=mode, clock=clock, timezone="1")
        ns = {"extract_config_body": lambda raw: raw,
              "parse_readconfig": lambda raw: state,
              "build_saveconfig": lambda _: expected + ("9" if mismatch else "")}
        exec(compile(ast.Module(body=[method], type_ignores=[]), str(source), "exec"), ns)
        client = SimpleNamespace(_request_lock=asyncio.Lock(), _serial="demo",
                                 _fetch_locked=AsyncMock(return_value="config"),
                                 _set_conn=lambda _: None, _on_message=lambda _: None)
        commands = ["save=" + expected, "clock=39600"]  # UTC 11:00 + device UTC+1
        async def prepare(_):
            return commands
        await ns["async_studio_exchange"](client, prepare)
        self.assertEqual([c.args[0] for c in client._fetch_locked.call_args_list],
                         ["read=config", *commands, "read=config"])

    async def test_verified_save_and_clock(self):
        await self.exchange()

    async def test_silent_curve_change_is_rejected(self):
        with self.assertRaises(OSError):
            await self.exchange(mismatch=True)

    async def test_one_hour_clock_error_is_rejected(self):
        with self.assertRaises(OSError):
            await self.exchange(clock="13,00")

    async def test_wrong_fixture_is_rejected_before_write(self):
        with self.assertRaises(OSError):
            await self.exchange(serial="other")


if __name__ == "__main__":
    unittest.main()
