"""Switch platform: a master power control for the whole fixture.

The fixture has no single firmware power command exposed to the app (a
saveconfig always forces power on). This master switch therefore works the
same way the app's manual controls do: OFF drives every channel to 0, ON
restores the last non-zero levels. Fully reversible, no schedule changes.
"""
from __future__ import annotations

from typing import Any

from homeassistant.components.switch import SwitchEntity
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers.entity_platform import AddEntitiesCallback

from .const import (
    CONF_ACCLIM_START,
    CONF_ACCLIM_WEEKS,
    CONF_NIGHT_FEED_CHANNELS,
    CONF_NIGHT_FEED_LEVEL,
    DEFAULT_ACCLIM_START,
    DEFAULT_ACCLIM_WEEKS,
    DEFAULT_NIGHT_FEED_LEVEL,
    DOMAIN,
)
from .hub import AipaiLightHub


async def async_setup_entry(
    hass: HomeAssistant, entry: ConfigEntry, async_add_entities: AddEntitiesCallback
) -> None:
    hub = hass.data[DOMAIN][entry.entry_id]
    if isinstance(hub, AipaiLightHub):
        if hub.local:
            async_add_entities([AipaiIndicatorSwitch(hub), AipaiPhysicalResumeSwitch(hub)])
        return  # AiPai Studio replaces the legacy light switches.
    from .experimental import ExperimentalDeviceHub
    from .experimental_entities import build_entities

    if isinstance(hub, ExperimentalDeviceHub):
        async_add_entities(build_entities(hub, "switch"))


class AipaiIndicatorSwitch(SwitchEntity):
    """PilotLamp; optimistic because firmware has no documented LED readback."""
    _attr_should_poll = False
    _attr_has_entity_name = True
    _attr_name = "Tastenbeleuchtung"
    _attr_icon = "mdi:led-on"
    _attr_assumed_state = True

    def __init__(self, hub: AipaiLightHub) -> None:
        self._hub = hub
        self._attr_unique_id = f"{hub.serial}_indicator_light"
        self._attr_device_info = hub.device_info
        hub.register_entity(self)

    @property
    def available(self) -> bool:
        return self._hub.local and self._hub.available

    @property
    def is_on(self) -> bool | None:
        return self._hub.indicator_light

    async def async_turn_on(self, **kwargs: Any) -> None:
        await self._hub.async_set_indicator_light(True)

    async def async_turn_off(self, **kwargs: Any) -> None:
        await self._hub.async_set_indicator_light(False)


class AipaiPhysicalResumeSwitch(SwitchEntity):
    """Optimistic turnoffset control; no state field exists in readconfig."""
    _attr_should_poll = False
    _attr_has_entity_name = True
    _attr_name = "Profil nach physischem Ausschalten fortsetzen"
    _attr_icon = "mdi:restart"
    _attr_assumed_state = True

    def __init__(self, hub: AipaiLightHub) -> None:
        self._hub = hub
        self._attr_unique_id = f"{hub.serial}_physical_resume"
        self._attr_device_info = hub.device_info
        hub.register_entity(self)

    @property
    def available(self) -> bool:
        return self._hub.local and self._hub.available

    @property
    def is_on(self) -> bool | None:
        return self._hub.physical_resume

    async def async_turn_on(self, **kwargs: Any) -> None:
        await self._hub.async_set_physical_resume(True)

    async def async_turn_off(self, **kwargs: Any) -> None:
        await self._hub.async_set_physical_resume(False)


class AipaiMasterSwitch(SwitchEntity):
    _attr_should_poll = False
    _attr_has_entity_name = True
    _attr_name = "Power"
    _attr_icon = "mdi:lightbulb-group"

    def __init__(self, hub: AipaiLightHub) -> None:
        self._hub = hub
        self._attr_unique_id = f"{hub.serial}_power"
        self._attr_device_info = hub.device_info
        hub.register_entity(self)

    @property
    def available(self) -> bool:
        return self._hub.available

    @property
    def is_on(self) -> bool:
        return self._hub.is_on

    async def async_turn_on(self, **kwargs: Any) -> None:
        self._hub.turn_all_on()

    async def async_turn_off(self, **kwargs: Any) -> None:
        self._hub.turn_all_off()


class AipaiNightFeedingSwitch(SwitchEntity):
    """A dim-red "night feeding" look: manual red-only on, schedule resumes off.

    On: the fixture goes to manual mode with only its red channel(s) lit at a
    configurable level (Options), so corals stay closed but the tank is visible.
    Off: the stored schedule simply resumes - the device's own curve is the undo.
    """

    _attr_should_poll = False
    _attr_has_entity_name = True
    _attr_name = "Night feeding"
    _attr_icon = "mdi:food-drumstick"

    def __init__(self, hub: AipaiLightHub, entry: ConfigEntry) -> None:
        self._hub = hub
        self._entry = entry
        self._attr_unique_id = f"{hub.serial}_night_feeding"
        self._attr_device_info = hub.device_info
        hub.register_entity(self)

    @property
    def available(self) -> bool:
        return self._hub.available

    @property
    def is_on(self) -> bool:
        return self._hub.is_night_feeding

    async def async_turn_on(self, **kwargs: Any) -> None:
        opts = self._entry.options
        level = int(opts.get(CONF_NIGHT_FEED_LEVEL, DEFAULT_NIGHT_FEED_LEVEL))
        channels = opts.get(CONF_NIGHT_FEED_CHANNELS) or None
        await self._hub.async_start_night_feeding(level, channels)

    async def async_turn_off(self, **kwargs: Any) -> None:
        await self._hub.async_stop_night_feeding()


class AipaiAcclimationSwitch(SwitchEntity):
    """Acclimation ramp: dim the whole schedule and climb to 100% over weeks.

    On: start the ramp from the configured start level, growing daily. Off:
    jump straight back to the full schedule. Start level and length are set per
    light under Configure.
    """

    _attr_should_poll = False
    _attr_has_entity_name = True
    _attr_name = "Acclimation"
    _attr_icon = "mdi:trending-up"

    def __init__(self, hub: AipaiLightHub, entry: ConfigEntry) -> None:
        self._hub = hub
        self._entry = entry
        self._attr_unique_id = f"{hub.serial}_acclimation"
        self._attr_device_info = hub.device_info
        hub.register_entity(self)

    @property
    def available(self) -> bool:
        return self._hub.available

    @property
    def is_on(self) -> bool:
        return self._hub.is_acclimating

    async def async_turn_on(self, **kwargs: Any) -> None:
        opts = self._entry.options
        start_pct = int(opts.get(CONF_ACCLIM_START, DEFAULT_ACCLIM_START))
        weeks = int(opts.get(CONF_ACCLIM_WEEKS, DEFAULT_ACCLIM_WEEKS))
        await self._hub.async_start_acclimation(start_pct, weeks)

    async def async_turn_off(self, **kwargs: Any) -> None:
        await self._hub.async_stop_acclimation()
