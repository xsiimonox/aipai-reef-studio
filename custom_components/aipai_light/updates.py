"""GitHub release checks shared by Reef Studio and the HA update entity."""
from __future__ import annotations

import json
import logging
import time
from datetime import timedelta
from pathlib import Path

from aiohttp import ClientError, ClientTimeout
from homeassistant.helpers.aiohttp_client import async_get_clientsession
from homeassistant.helpers.update_coordinator import DataUpdateCoordinator

from .const import DOMAIN

_LOGGER = logging.getLogger(__name__)
_RELEASE_URL = "https://api.github.com/repos/xsiimonox/aipai-reef-studio/releases/latest"
_CACHE_SECONDS = 30 * 60
_KEY = f"{DOMAIN}_update_status"
_COORDINATOR_KEY = f"{DOMAIN}_release_coordinator"


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


async def async_get_latest_release(hass) -> dict:  # noqa: ANN001
    """Read the newest published GitHub release, never the mutable main branch."""
    session = async_get_clientsession(hass)
    async with session.get(
        _RELEASE_URL,
        headers={"Accept": "application/vnd.github+json"},
        timeout=ClientTimeout(total=8),
    ) as response:
        response.raise_for_status()
        payload = await response.json(content_type=None)
    version = payload.get("tag_name", "").removeprefix("v")
    if not isinstance(version, str) or not _version_tuple(version):
        raise ValueError("Ungültige Versionsinformation im GitHub-Release")
    return {
        "version": version,
        "url": payload.get("html_url"),
        "notes": payload.get("body") or "",
        "published_at": payload.get("published_at"),
    }


async def _async_refresh(hass, status: dict) -> None:  # noqa: ANN001
    try:
        release = await async_get_latest_release(hass)
        local = _local_version()
        status.update({
            "current": local,
            "latest": release["version"],
            "url": release["url"],
            "available": _version_tuple(release["version"]) > _version_tuple(local),
        })
    except (ClientError, TimeoutError, ValueError, json.JSONDecodeError):
        # Version checks must never make the local integration noisy or fail.
        status["available"] = False
    finally:
        status["checked"] = time.monotonic()
        status["checking"] = False


async def async_get_release_coordinator(hass) -> DataUpdateCoordinator:  # noqa: ANN001
    """Return a shared 30-minute GitHub-release coordinator for HA updates."""
    coordinator = hass.data.get(_COORDINATOR_KEY)
    if coordinator is not None:
        return coordinator

    async def _async_update_data() -> dict:
        try:
            return await async_get_latest_release(hass)
        except (ClientError, TimeoutError, ValueError, json.JSONDecodeError) as err:
            # An offline Home Assistant must not make the integration fail to
            # load; the next periodic check will try again.
            _LOGGER.debug("Could not check AIPAI Reef Studio release: %s", err)
            return {"version": None, "url": None, "notes": "", "published_at": None}

    coordinator = DataUpdateCoordinator(
        hass,
        _LOGGER,
        name="AIPAI Reef Studio release",
        update_method=_async_update_data,
        update_interval=timedelta(minutes=30),
    )
    hass.data[_COORDINATOR_KEY] = coordinator
    await coordinator.async_refresh()
    return coordinator


def get_update_status(hass) -> dict:  # noqa: ANN001
    """Return cached panel status and refresh it in the background when stale."""
    status = hass.data.setdefault(_KEY, {"current": _local_version(), "available": False, "checked": 0.0})
    if not status.get("checking") and time.monotonic() - status["checked"] >= _CACHE_SECONDS:
        status["checking"] = True
        hass.async_create_background_task(_async_refresh(hass, status), "aipai_update_check")
    return {key: status.get(key) for key in ("current", "latest", "available", "checking", "url")}
