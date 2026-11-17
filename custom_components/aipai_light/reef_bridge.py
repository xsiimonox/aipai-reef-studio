"""Local Home Assistant representation of an AIPAI Reef Bridge."""
from __future__ import annotations

import asyncio
from typing import Any

import aiohttp
from homeassistant.helpers.aiohttp_client import async_get_clientsession
from homeassistant.helpers.entity import DeviceInfo

from .const import DOMAIN


class ReefBridgeHub:
    """Read-only local status and device metadata for one Reef Bridge."""

    def __init__(self, hass, bridge_id: str, host: str, name: str | None = None) -> None:  # noqa: ANN001
        self.hass = hass
        self.bridge_id = bridge_id
        self.host = host
        self.name = name or "Reef Bridge"
        self.available = False
        self.data: dict[str, Any] = {}
        self._entities: list[Any] = []

    @property
    def url(self) -> str:
        return f"http://{self.host}"

    @property
    def device_info(self) -> DeviceInfo:
        return DeviceInfo(
            identifiers={(DOMAIN, f"reef_bridge_{self.bridge_id}")},
            name=self.name,
            manufacturer="AiPai Studio",
            model="AIPAI Reef Bridge (ESP32)",
            serial_number=self.bridge_id,
            sw_version=str(self.data.get("version") or "Unknown"),
            configuration_url=self.url,
        )

    def register_entity(self, entity: Any) -> None:
        self._entities.append(entity)

    async def async_connect(self) -> None:
        await self.async_refresh()
        if not self.available:
            raise OSError(f"Reef Bridge at {self.host} is not reachable")

    async def async_disconnect(self) -> None:
        self._entities.clear()

    async def async_refresh(self) -> None:
        try:
            session = async_get_clientsession(self.hass)
            async with asyncio.timeout(6):
                async with session.get(f"{self.url}/api/status") as response:
                    response.raise_for_status()
                    data = await response.json(content_type=None)
            if not isinstance(data, dict):
                raise ValueError("invalid Bridge status")
            self.data = data
            self.available = True
        except (aiohttp.ClientError, OSError, TimeoutError, ValueError):
            self.available = False
        for entity in list(self._entities):
            if getattr(entity, "hass", None) is not None:
                entity.async_write_ha_state()
