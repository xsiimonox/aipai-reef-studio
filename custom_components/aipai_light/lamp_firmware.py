"""Read vendor firmware metadata; fail closed for unauthenticated OTA images.

APK 3.1.93: public.js ReadDevsFromServer uses d_class/d_ver; ctrl-light.html
firmware() reads tbl_class.status/remark. Neither is local version attestation.
"""
from __future__ import annotations

import asyncio
import json
import re
import time
from datetime import datetime, timezone

from aiohttp import ClientError, ClientTimeout
from homeassistant.helpers.aiohttp_client import async_get_clientsession

ENDPOINT = "http://ahome.doseen.com/api.ashx"
INSTALL_BLOCK = "Installation gesperrt: Herstellerweg ohne belegte Firmware-Signaturprüfung und ohne verifizierte lokale Versionskontrolle."


def identifier(value) -> str:
    if not isinstance(value, str) or not re.fullmatch(r"[A-Za-z0-9_-]{1,64}", value):
        raise ValueError("Ungültige Gerätekennung für die Herstellerabfrage.")
    return value


def version(value) -> str:
    text = str(value).strip()
    if not re.fullmatch(r"[0-9]{1,10}(?:\.[0-9]{1,10}){0,3}", text):
        raise ValueError("Hersteller liefert keine gültige Firmwareversion.")
    return text


def version_key(value: str) -> tuple:
    parts = tuple(int(p) for p in value.split("."))
    return parts + (0,) * (4 - len(parts))


def device_metadata(rows, serial: str) -> dict:
    if not isinstance(rows, list) or len(rows) != 1 or not isinstance(rows[0], dict):
        raise ValueError("Kein eindeutiger Herstellerdatensatz für diese Lampe.")
    row = rows[0]
    if str(row.get("d_sn", "")).strip() != serial:
        raise ValueError("Seriennummer in Herstellerantwort stimmt nicht überein.")
    return {"vendor_model": identifier(row.get("d_class")), "manufacturer_version": version(row.get("d_ver"))}


def release_metadata(rows, model: str) -> dict:
    if not isinstance(rows, list) or len(rows) != 1 or not isinstance(rows[0], dict) or rows[0].get("classname") != model:
        raise ValueError("Keine eindeutige Firmware für dieses Herstellermodell.")
    return {"latest": version(rows[0].get("status")), "notes": str(rows[0].get("remark") or "")[:2000]}


async def check_all_lamps(studios):
    """Check every fixture, retaining per-lamp failures and rate limits."""
    for studio in studios:
        try:
            await studio.command({"action": "firmware_check"})
        except (OSError, ValueError) as err:
            studio.hub.log_activity("Lampen-Firmware-Prüfung übersprungen", str(err), "warning")


class LampFirmware:
    """User-triggered metadata queries only; never send version= to a fixture."""

    def __init__(self, hub):
        self.hub = hub
        self.lock = asyncio.Lock()
        self.last_attempt = None
        self.data = self.empty()

    def empty(self):
        return {"manufacturer_version": None, "latest": None, "vendor_model": None,
                "local_version": None, "available": None, "checked_at": None,
                "source": "manufacturer_http", "install_allowed": False,
                "block_reason": INSTALL_BLOCK, "error": None, "notes": ""}

    async def query(self, sql):
        session = async_get_clientsession(self.hub.hass)
        async with session.get(ENDPOINT, params={"action": "data_read", "sql": sql},
                               allow_redirects=False, timeout=ClientTimeout(total=8)) as response:
            if response.status != 200:
                raise ValueError(f"Herstellerserver antwortet mit HTTP {response.status}.")
            raw = bytearray()
            async for chunk in response.content.iter_chunked(4096):
                raw.extend(chunk)
                if len(raw) > 32768:
                    raise ValueError("Herstellerantwort ist zu groß.")
            return json.loads(raw.decode("utf-8-sig"))

    async def check(self):
        if self.lock.locked():
            raise ValueError("Versionsprüfung läuft bereits.")
        async with self.lock:
            if self.last_attempt is not None and time.monotonic() - self.last_attempt < 60:
                raise ValueError("Bitte mindestens eine Minute zwischen Herstellerabfragen warten.")
            self.last_attempt = time.monotonic()
            self.data = self.empty()
            try:
                serial = identifier(self.hub.serial)
                device = device_metadata(await self.query(
                    f"select d_sn,d_class,d_ver from tbl_device where d_sn='{serial}'"), serial)
                self.data.update(device)
                model = device["vendor_model"]
                release = release_metadata(await self.query(
                    f"select classname,status,remark from tbl_class where classname='{model}'"), model)
                self.data.update(device, **release)
                self.data["available"] = version_key(release["latest"]) > version_key(device["manufacturer_version"])
                self.hub.log_activity("Lampen-Firmware geprüft", f"Hersteller meldet {device['manufacturer_version']}; verfügbar {release['latest']}; keine lokale Versionsbestätigung")
            except (ClientError, TimeoutError, ValueError, UnicodeError) as err:
                self.data["error"] = str(err) or "Herstellerserver nicht erreichbar."
                self.hub.log_activity("Lampen-Firmware-Abfrage fehlgeschlagen", self.data["error"], "warning")
            finally:
                self.data["checked_at"] = datetime.now(timezone.utc).isoformat()

    async def install(self):
        # Enforcement belongs on the server, not merely in a disabled UI button.
        self.hub.log_activity("Firmwareinstallation blockiert", INSTALL_BLOCK, "warning")
        raise ValueError(INSTALL_BLOCK)
