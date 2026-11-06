"""Best-effort update information for the Reef Studio panel.

This never participates in fixture control.  It only reads the public GitHub
manifest occasionally so that the panel can point users to a HACS update.
"""
from __future__ import annotations

import json
import time
from pathlib import Path

from aiohttp import ClientError, ClientTimeout
from homeassistant.helpers.aiohttp_client import async_get_clientsession

from .const import DOMAIN

_MANIFEST_URL = (
    "https://raw.githubusercontent.com/xsiimonox/aipai-reef-studio/main/"
    "custom_components/aipai_light/manifest.json"
)
_CACHE_SECONDS = 6 * 60 * 60
_KEY = f"{DOMAIN}_update_status"


def _version_tuple(version: str) -> tuple[int, int, int] | None:
    parts = version.split(".")
    if len(parts) != 3 or not all(part.isdigit() for part in parts):
        return None
    return tuple(int(part) for part in parts)  # type: ignore[return-value]


def _local_version() -> str:
    manifest = Path(__file__).with_name("manifest.json")
    try:
        return str(json.loads(manifest.read_text(encoding="utf-8"))["version"])
    except (OSError, KeyError, json.JSONDecodeError):
        return "unbekannt"


async def _async_refresh(hass, status: dict) -> None:  # noqa: ANN001
    try:
        session = async_get_clientsession(hass)
        async with session.get(_MANIFEST_URL, timeout=ClientTimeout(total=5)) as response:
            response.raise_for_status()
            remote = (await response.json(content_type=None)).get("version")
        local = _local_version()
        if not isinstance(remote, str) or not _version_tuple(remote) or not _version_tuple(local):
            raise ValueError("Ungültige Versionsinformation")
        status.update({"current": local, "latest": remote, "available": _version_tuple(remote) > _version_tuple(local)})
    except (ClientError, TimeoutError, ValueError, json.JSONDecodeError):
        # Version checks must never make the local integration noisy or fail.
        status["available"] = False
    finally:
        status["checked"] = time.monotonic()
        status["checking"] = False


def get_update_status(hass) -> dict:  # noqa: ANN001
    """Return cached update state and refresh it in the background if stale."""
    status = hass.data.setdefault(_KEY, {"current": _local_version(), "available": False, "checked": 0.0})
    if not status.get("checking") and time.monotonic() - status["checked"] >= _CACHE_SECONDS:
        status["checking"] = True
        hass.async_create_background_task(_async_refresh(hass, status), "aipai_update_check")
    return {key: status.get(key) for key in ("current", "latest", "available", "checking")}
