"""Home Assistant update entity for published AIPAI Reef Studio releases."""
from __future__ import annotations

from homeassistant.components.update import UpdateEntity, UpdateEntityFeature
from homeassistant.config_entries import ConfigEntry
from homeassistant.const import EntityCategory
from homeassistant.core import HomeAssistant
from homeassistant.helpers.entity_platform import AddEntitiesCallback
from homeassistant.helpers.update_coordinator import CoordinatorEntity

from .const import DOMAIN
from .hub import AipaiLightHub
from .updates import _local_version, async_get_release_coordinator


async def async_setup_entry(
    hass: HomeAssistant, entry: ConfigEntry, async_add_entities: AddEntitiesCallback
) -> None:
    hub = hass.data[DOMAIN][entry.entry_id]
    if not isinstance(hub, AipaiLightHub):
        return
    coordinator = await async_get_release_coordinator(hass)
    async_add_entities([AipaiReefStudioUpdate(hub, coordinator)])


class AipaiReefStudioUpdate(CoordinatorEntity, UpdateEntity):
    """Expose published releases in Settings > System > Updates."""

    _attr_has_entity_name = True
    _attr_name = "Reef Studio update"
    _attr_entity_category = EntityCategory.DIAGNOSTIC
    _attr_supported_features = UpdateEntityFeature.RELEASE_NOTES
    _attr_auto_update = False
    _attr_in_progress = False

    def __init__(self, hub: AipaiLightHub, coordinator) -> None:  # noqa: ANN001
        super().__init__(coordinator)
        self._attr_unique_id = f"{hub.serial}_reef_studio_update"
        self._attr_device_info = hub.device_info

    @property
    def installed_version(self) -> str:
        return _local_version()

    @property
    def latest_version(self) -> str:
        latest = self.coordinator.data.get("version") if self.coordinator.data else None
        return latest if isinstance(latest, str) else self.installed_version

    @property
    def release_url(self) -> str | None:
        url = self.coordinator.data.get("url") if self.coordinator.data else None
        return url if isinstance(url, str) else None

    @property
    def available(self) -> bool:
        return self.coordinator.last_update_success

    @property
    def title(self) -> str:
        return "AIPAI Reef Studio"

    async def async_release_notes(self) -> str | None:
        notes = self.coordinator.data.get("notes") if self.coordinator.data else ""
        return notes if isinstance(notes, str) and notes else None

