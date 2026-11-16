"""Run production metadata validation and installation guards without devices."""
import ast
import asyncio
from datetime import datetime, timezone
import json
from pathlib import Path
import re
import time
from types import SimpleNamespace
import unittest
from unittest.mock import AsyncMock, Mock

source = Path(__file__).resolve().parents[1] / "custom_components/aipai_light/lamp_firmware.py"
tree = ast.parse(source.read_text())
ns = dict(asyncio=asyncio, datetime=datetime, timezone=timezone, json=json, re=re, time=time,
          ClientError=ConnectionError, ClientTimeout=lambda **kwargs: kwargs)
exec(compile(ast.Module(body=[n for n in tree.body if not isinstance(n, (ast.Import, ast.ImportFrom))], type_ignores=[]), str(source), "exec"), ns)


class FirmwareTests(unittest.IsolatedAsyncioTestCase):
    async def test_bulk_check_continues_after_a_failed_or_rate_limited_lamp(self):
        first = SimpleNamespace(command=AsyncMock(side_effect=ValueError("Bitte warten")), hub=SimpleNamespace(log_activity=Mock()))
        second = SimpleNamespace(command=AsyncMock(), hub=SimpleNamespace(log_activity=Mock()))
        await ns["check_all_lamps"]([first, second])
        first.command.assert_awaited_once_with({"action": "firmware_check"})
        second.command.assert_awaited_once_with({"action": "firmware_check"})
        first.hub.log_activity.assert_called_once()

    def firmware(self, serial="demo"):
        hub = SimpleNamespace(serial=serial, hass=None, log_activity=Mock(), client=SimpleNamespace())
        result = ns["LampFirmware"](hub)
        result.query = AsyncMock(side_effect=[
            [{"d_sn": serial, "d_class": "A8-SE8", "d_ver": "20250101"}],
            [{"classname": "A8-SE8", "status": "20260601", "remark": "Notes"}],
        ])
        return result

    async def test_cloud_version_is_not_a_local_attestation(self):
        firmware = self.firmware()
        await firmware.check()
        self.assertTrue(firmware.data["available"])
        self.assertEqual(firmware.data["manufacturer_version"], "20250101")
        self.assertIsNone(firmware.data["local_version"])
        self.assertFalse(firmware.data["install_allowed"])
        self.assertEqual(firmware.data["source"], "manufacturer_http")

    async def test_installation_blocked_even_after_update_found(self):
        firmware = self.firmware()
        await firmware.check()
        with self.assertRaisesRegex(ValueError, "Installation gesperrt"):
            await firmware.install()
        self.assertEqual(firmware.query.await_count, 2)

    async def test_identity_mismatch_stops_before_release_lookup(self):
        firmware = self.firmware()
        firmware.query = AsyncMock(return_value=[{"d_sn": "other", "d_class": "A8-SE8", "d_ver": "1"}])
        await firmware.check()
        self.assertIsNone(firmware.data["available"])
        self.assertIsNotNone(firmware.data["error"])
        self.assertEqual(firmware.query.await_count, 1)

    async def test_invalid_serial_cannot_change_vendor_query(self):
        firmware = self.firmware("' OR 1=1")
        await firmware.check()
        firmware.query.assert_not_awaited()

    async def test_offline_does_not_report_up_to_date(self):
        firmware = self.firmware()
        firmware.query = AsyncMock(side_effect=TimeoutError())
        await firmware.check()
        self.assertIsNone(firmware.data["available"])
        self.assertIsNone(firmware.data["latest"])
        self.assertTrue(firmware.data["error"])

    async def test_repeated_requests_are_rate_limited(self):
        firmware = self.firmware()
        await firmware.check()
        with self.assertRaises(ValueError):
            await firmware.check()
        self.assertEqual(firmware.query.await_count, 2)

    def test_versions_and_model_validation(self):
        self.assertGreater(ns["version_key"]("1.10"), ns["version_key"]("1.9"))
        self.assertEqual(ns["version_key"]("1"), ns["version_key"]("1.0"))
        with self.assertRaises(ValueError):
            ns["release_metadata"]([{"classname": "wrong", "status": "2"}], "A8-SE8")
        with self.assertRaises(ValueError):
            ns["version"]("unknown")


if __name__ == "__main__":
    unittest.main()
