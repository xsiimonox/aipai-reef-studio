"""Minute-resolution lighting and scenes for the native AiPai Studio panel."""
from __future__ import annotations

import asyncio
from dataclasses import replace
from typing import Any
from uuid import uuid4

import voluptuous as vol
from homeassistant.components import websocket_api
from homeassistant.helpers.event import async_call_later, async_track_time_change
from homeassistant.helpers.storage import Store
from homeassistant.util import dt as dt_util

from .const import DOMAIN, ROAD_CODES
from .hub import AipaiLightHub
from .protocol import build_saveconfig, parse_readconfig, pct_to_cmd

DAY = 1440


def levels_at(points: list[dict], minute: int) -> list[int]:
    """Interpolate across midnight as well as between daytime points."""
    right = next((i for i, p in enumerate(points) if p["minute"] > minute), 0)
    start, end = points[right - 1], points[right]
    span = (end["minute"] - start["minute"]) % DAY
    fraction = ((minute - start["minute"]) % DAY) / span
    return [
        int(a + (b - a) * fraction + 0.5)
        for a, b in zip(start["levels"], end["levels"], strict=True)
    ]


def validate_levels(value: Any, count: int) -> list[int]:
    if not isinstance(value, list) or len(value) != count or any(
        type(level) is not int or not 0 <= level <= 100 for level in value
    ):
        raise ValueError(f"Genau {count} ganzzahlige Kanalwerte zwischen 0 und 100 erwartet.")
    return value


def validate_profile_levels(value: Any) -> list[int]:
    """Validate a reusable profile without binding it to one lamp model."""
    if not isinstance(value, list) or not 1 <= len(value) <= len(ROAD_CODES) or any(
        type(level) is not int or not 0 <= level <= 100 for level in value
    ):
        raise ValueError(f"1 bis {len(ROAD_CODES)} ganzzahlige Kanalwerte zwischen 0 und 100 erwartet.")
    return value


def fit_levels(levels: list[int], count: int) -> list[int]:
    """Apply a profile to a fixture, zero-filling channels it does not contain."""
    return [levels[index] if index < len(levels) else 0 for index in range(count)]


def firmware_schedule_rows(
    points: list[dict], exposed_roads: int, firmware_roads: int
) -> list[str]:
    """Sample a Studio profile into the lamp's persistent 24-hour curves.

    AIPAI firmware stores one value per full hour and interpolates between
    those values itself. Hidden firmware roads are deliberately written as
    zero so a six-channel fixture never receives UV/warm-white output.
    """
    hourly = [fit_levels(levels_at(points, hour * 60), exposed_roads) for hour in range(24)]
    return [
        ",".join(str(hourly[hour][road] if road < exposed_roads else 0) for hour in range(24))
        for road in range(firmware_roads)
    ]


def needs_startup_hold(data: dict) -> bool:
    """Whether a persisted override would write after an unattended restart."""
    return bool(
        data.get("scene") or data.get("preview") or data.get("restore")
        or (data.get("program") and data.get("program_execution") != "device")
    )


def validate_item(kind: str, item: dict, count: int) -> dict:
    name = item.get("name", "")
    if not isinstance(name, str) or not 1 <= len(name.strip()) <= 64:
        raise ValueError("Bitte einen Namen mit 1 bis 64 Zeichen eingeben.")
    if kind == "scenes":
        return {"name": name.strip(), "levels": validate_levels(item.get("levels"), count)}
    points = item.get("points")
    if not isinstance(points, list) or not 2 <= len(points) <= 48:
        raise ValueError("Ein Programm braucht 2 bis 48 Stützpunkte.")
    validated = []
    for point in points:
        minute = point.get("minute") if isinstance(point, dict) else None
        if type(minute) is not int or not 0 <= minute < DAY:
            raise ValueError("Jeder Stützpunkt braucht eine Uhrzeit von 00:00 bis 23:59.")
        validated.append({
            # Older libraries may still contain a curve field. It is retained
            # as linear only; the profile no longer supports curve choices.
            "minute": minute, "levels": validate_levels(point.get("levels"), count), "curve": "linear",
        })
    if len({point["minute"] for point in validated}) != len(validated):
        raise ValueError("Jede Uhrzeit darf nur einmal vorkommen.")
    return {
        "name": name.strip(), "points": sorted(validated, key=lambda point: point["minute"]),
    }


def validate_library_item(kind: str, item: dict) -> dict:
    """Validate a shared profile and retain its channel count for other lamps."""
    name = item.get("name", "")
    if not isinstance(name, str) or not 1 <= len(name.strip()) <= 64:
        raise ValueError("Bitte einen Namen mit 1 bis 64 Zeichen eingeben.")
    if kind == "scenes":
        return {"name": name.strip(), "levels": validate_profile_levels(item.get("levels"))}
    points = item.get("points")
    if not isinstance(points, list) or not 2 <= len(points) <= 48:
        raise ValueError("Ein Programm braucht 2 bis 48 Stützpunkte.")
    validated = []
    channels = None
    for point in points:
        minute = point.get("minute") if isinstance(point, dict) else None
        if type(minute) is not int or not 0 <= minute < DAY:
            raise ValueError("Jeder Stützpunkt braucht eine Uhrzeit von 00:00 bis 23:59.")
        levels = validate_profile_levels(point.get("levels"))
        if channels is None:
            channels = len(levels)
        elif len(levels) != channels:
            raise ValueError("Alle Stützpunkte eines Programms brauchen dieselbe Kanalanzahl.")
        validated.append({"minute": minute, "levels": levels, "curve": "linear"})
    if len({point["minute"] for point in validated}) != len(validated):
        raise ValueError("Jede Uhrzeit darf nur einmal vorkommen.")
    return {"name": name.strip(), "points": sorted(validated, key=lambda point: point["minute"])}


class ReefLibrary:
    """One shared program and scene library for every AiPai Studio fixture."""

    def __init__(self, hass) -> None:  # noqa: ANN001
        self.store = Store(hass, 1, f"{DOMAIN}.studio_library")
        self.data = {"programs": {}, "scenes": {}}
        self.lock = asyncio.Lock()
        self.loaded = False

    async def load(self) -> None:
        async with self.lock:
            if self.loaded:
                return
            stored = await self.store.async_load() or {}
            self.data = {
                kind: stored.get(kind, {}) if isinstance(stored.get(kind), dict) else {}
                for kind in ("programs", "scenes")
            }
            self.loaded = True

    async def import_legacy(self, data: dict) -> dict[str, dict[str, str]]:
        """Import the old per-lamp library once, preserving active references."""
        await self.load()
        mapping = {"programs": {}, "scenes": {}}
        changed = False
        async with self.lock:
            for kind in ("programs", "scenes"):
                for item_id, item in data.get(kind, {}).items():
                    try:
                        item = validate_library_item(kind, item)
                    except (TypeError, ValueError):
                        continue
                    target_id = item_id
                    if target_id in self.data[kind] and self.data[kind][target_id] != item:
                        target_id = uuid4().hex
                    mapping[kind][item_id] = target_id
                    if target_id not in self.data[kind]:
                        self.data[kind][target_id] = item
                        changed = True
            if changed:
                await self.store.async_save(self.data)
        return mapping

    async def save_item(self, kind: str, item_id: str | None, item: dict) -> str:
        await self.load()
        validated = validate_library_item(kind, item)
        async with self.lock:
            if item_id is None and len(self.data[kind]) >= 100:
                raise ValueError("Maximal 100 Einträge pro Bibliothek.")
            item_id = item_id or uuid4().hex
            self.data[kind][item_id] = validated
            await self.store.async_save(self.data)
        return item_id

    async def delete_item(self, kind: str, item_id: str) -> None:
        await self.load()
        async with self.lock:
            if item_id not in self.data[kind]:
                raise ValueError("Der Eintrag wurde bereits gelöscht.")
            del self.data[kind][item_id]
            await self.store.async_save(self.data)


async def async_get_library(hass) -> ReefLibrary:  # noqa: ANN001
    key = f"{DOMAIN}_studio_library"
    library = hass.data.get(key)
    if library is None:
        library = ReefLibrary(hass)
        hass.data[key] = library
    await library.load()
    return library


class ReefStudio:
    """Persistent library and a serialized controller for one local fixture."""

    def __init__(self, hub: AipaiLightHub) -> None:
        self.hub = hub
        self.store = Store(hub.hass, 1, f"{DOMAIN}.studio.{hub.serial}")
        self.data = {
            "programs": {}, "scenes": {}, "program": None, "scene": None,
            "preview": None, "restore": None, "program_execution": None,
        }
        self.lock = asyncio.Lock()
        self.sent: list[int] | None = None
        self.sent_at: str | None = None
        self.error: str | None = None
        self.unsubscribe = None
        self.expiry_cancel = None
        self.closed = False
        self.startup_hold = False
        self.library: ReefLibrary | None = None

    async def start(self) -> None:
        self.data.update(await self.store.async_load() or {})
        self.library = await async_get_library(self.hub.hass)
        changed = False
        if not self.data.get("shared_library_migrated"):
            mapping = await self.library.import_legacy(self.data)
            if self.data.get("program") in mapping["programs"]:
                self.data["program"] = mapping["programs"][self.data["program"]]
            if self.data.get("scene") and self.data["scene"].get("id") in mapping["scenes"]:
                self.data["scene"] = self.data["scene"] | {
                    "id": mapping["scenes"][self.data["scene"]["id"]]
                }
            self.data["shared_library_migrated"] = True
            changed = True
        # Releases before 0.22 ran an active program from HA in manual mode.
        # Never resume that write loop merely because HA restarted: retain the
        # actual lamp output until the user explicitly deploys the profile.
        if self.data.get("program") and self.data.get("program_execution") not in (
            "device", "ha", "hold"
        ):
            self.data["program_execution"] = "hold"
            changed = True
        self.startup_hold = needs_startup_hold(self.data)
        if changed:
            await self.store.async_save(self.data)
        if self.startup_hold:
            self.hub.log_activity(
                "Sicherer Startmodus",
                "Keine Lichtwerte nach HA-Neustart gesendet; Profil bitte erneut auf der Lampe aktivieren.",
                "warning",
            )
        self._schedule_expiry()
        if self.hub.local:
            self.unsubscribe = async_track_time_change(self.hub.hass, self.tick, second=0)
            self.hub.hass.async_create_background_task(self.tick(), "aipai_studio_start")

    async def close(self) -> None:
        self.closed = True
        if self.unsubscribe:
            self.unsubscribe()
        if self.expiry_cancel:
            self.expiry_cancel()
            self.expiry_cancel = None
        async with self.lock:
            pass

    def snapshot(self) -> dict:
        now = self.hub._now()
        state = self.hub.state
        curves = [row.split(",") for row in state.road_data[:self.hub.roads]]
        firmware_points = []
        if self.hub.has_state and len(curves) == self.hub.roads and all(
            len(row) == 24 and all(value.isdigit() for value in row) for row in curves
        ):
            firmware_points = [
                {"minute": hour * 60, "levels": [min(100, int(row[hour])) for row in curves]}
                for hour in range(24)
            ]
        return {
            "programs": self.library.data["programs"] if self.library else {},
            "scenes": self.library.data["scenes"] if self.library else {},
            **{key: self.data[key] for key in ("program", "scene", "preview")},
            "program_execution": self.data.get("program_execution"),
            "startup_hold": self.startup_hold,
            "serial": self.hub.serial,
            "name": self.hub.display_name,
            "labels": self.hub.labels,
            "local": self.hub.local,
            "online": self.hub.available and self.hub.has_state,
            "minute": now.hour * 60 + now.minute,
            "timezone": str(now.tzinfo),
            "now_epoch": dt_util.utcnow().timestamp(),
            "mode": state.mode,
            "power": state.power,
            "temperature": state.temperature,
            "reported": state.road_val_pct[:self.hub.roads] if self.hub.has_state else [],
            "firmware_points": firmware_points,
            "sent": self.sent,
            "sent_at": self.sent_at,
            "error": self.error,
            "restoring": bool(self.data["restore"] and not (self.data["program"] or self.data["scene"])),
            "activity": list(self.hub.activity),
        }

    async def save(self, changes: dict) -> None:
        updated = self.data | changes
        await self.store.async_save(updated)
        self.data = updated
        self._schedule_expiry()

    def _schedule_expiry(self) -> None:
        """Wake precisely when a preview or temporary scene is due to end."""
        if self.expiry_cancel:
            self.expiry_cancel()
            self.expiry_cancel = None
        deadlines = [
            item["until"] for item in (self.data.get("preview"), self.data.get("scene"))
            if item and isinstance(item.get("until"), (int, float))
        ]
        if not deadlines or self.closed:
            return
        delay = max(0, min(deadlines) - dt_util.utcnow().timestamp())

        async def _expire(_now) -> None:  # noqa: ANN001
            self.expiry_cancel = None
            await self.tick()

        self.expiry_cancel = async_call_later(self.hub.hass, delay, _expire)

    def owns_output(self) -> bool:
        if self.startup_hold:
            return False
        return bool(
            self.data.get("scene") or self.data.get("preview") or self.data.get("restore")
            or (self.data.get("program") and self.data.get("program_execution") == "ha")
        )

    async def tick(self, _now=None) -> None:
        if self.closed or self.lock.locked() or self.startup_hold:
            return
        async with self.lock:
            try:
                # The fixture's local HTTP channel can drop a static manual
                # level after a few minutes. Re-send studio-controlled output
                # at every minute boundary, even on a flat program plateau.
                if self.data.get("scene") or self.data.get("preview") or (
                    self.data.get("program") and self.data.get("program_execution") == "ha"
                ):
                    self.sent = None
                await self.update_output()
            except (OSError, ValueError) as err:
                self.sent = None
                self.error = str(err)
                self.hub.log_activity("AiPai-Studio-Ausgabe fehlgeschlagen", str(err), "error")

    async def update_output(self) -> None:
        preview = self.data["preview"]
        if preview and preview["until"] <= dt_util.utcnow().timestamp():
            await self.save({"preview": None})
            preview = None
        scene = self.data["scene"]
        if scene and scene.get("until") is not None and scene["until"] <= dt_util.utcnow().timestamp():
            await self.save({"scene": None})
            scene = None
        program = self.data["program"] if self.data.get("program_execution") == "ha" else None
        restore = self.data["restore"]
        if not self.hub.local or not (program or scene or preview or restore):
            return
        if not self.hub.has_state:
            raise OSError("Warte auf den ersten Status der Lampe.")
        target = None
        if preview:
            target = validate_levels(preview["levels"], self.hub.roads)
        elif scene:
            target = fit_levels(
                validate_library_item("scenes", self.library.data["scenes"][scene["id"]])["levels"], self.hub.roads,
            )
        elif program:
            now = self.hub._now()
            validated = validate_library_item("programs", self.library.data["programs"][program])
            target = fit_levels(levels_at(validated["points"], now.hour * 60 + now.minute), self.hub.roads)

        async def prepare(body: str) -> list[str]:
            state = parse_readconfig(body)
            fields = body.split("|")
            if (
                state.mode not in ("0", "1") or self.hub.roads > state.roads_real
                or any(not field.strip().isdigit() for field in fields[2:5])
                or any(not row or any(not value.isdigit() for value in row.split(","))
                       for row in state.road_data)
                or not state.timezone.lstrip("-").isdigit()
            ):
                raise ValueError("Das Konfigurationsformat passt nicht zum Kanalprofil.")

            def save_query(mode: str, levels: list[int]) -> str:
                # Preserve the firmware curve and the live fan thresholds.
                tokens = build_saveconfig(replace(state, mode=mode, road_val_pct=levels)).split("x")
                tokens[2:5] = [field.strip() for field in fields[2:5]]
                return "save=" + "x".join(tokens)

            if target is None:
                return [save_query(restore["mode"], restore["levels"])]
            if not restore:
                if state.power != "on":
                    raise ValueError("Bitte die Lampe vor der Übernahme einschalten.")
                await self.save({"restore": {"mode": state.mode, "levels": state.road_val_pct}})
            if state.mode != "0" or state.power != "on":
                return [save_query("0", target + state.road_val_pct[len(target):])]
            return [
                f"{code}={pct_to_cmd(level)}"
                for index, (code, level) in enumerate(zip(ROAD_CODES, target))
                if not self.hub.available or self.sent is None or index >= len(self.sent)
                or level != self.sent[index]
            ]

        await self.hub.client.async_studio_exchange(prepare)
        if target is None:
            await self.save({"restore": None})
        if target != self.sent:
            self.sent_at = dt_util.utcnow().isoformat() if target is not None else None
        self.sent = target
        self.error = None

    async def deploy_program(self, item_id: str) -> None:
        """Persist a Studio program in the fixture and hand execution to it."""
        if not self.hub.local or not self.hub.has_state or not self.hub.available:
            raise OSError("Die Lampe ist gerade nicht lokal erreichbar.")
        if self.library is None or item_id not in self.library.data["programs"]:
            raise ValueError("Bitte ein gespeichertes Tagesprogramm wählen.")
        program = validate_library_item("programs", self.library.data["programs"][item_id])

        async def prepare(body: str) -> list[str]:
            state = parse_readconfig(body)
            fields = body.split("|")
            if (
                state.mode not in ("0", "1") or self.hub.roads > state.roads_real
                or any(not field.strip().isdigit() for field in fields[2:5])
                or any(not row or any(not value.isdigit() for value in row.split(","))
                       for row in state.road_data)
                or not state.timezone.lstrip("-").isdigit()
            ):
                raise ValueError("Das Konfigurationsformat passt nicht zum Kanalprofil.")
            if state.power != "on":
                raise ValueError("Bitte die Lampe vor dem Speichern einschalten.")
            rows = firmware_schedule_rows(program["points"], self.hub.roads, state.roads_real)
            deployed = replace(state, mode="1", road_data=rows)
            tokens = build_saveconfig(deployed).split("x")
            # Preserve the live fan thresholds reported by this exact fixture.
            tokens[2:5] = [field.strip() for field in fields[2:5]]
            return ["save=" + "x".join(tokens)]

        await self.hub.client.async_studio_exchange(prepare)
        self.startup_hold = False
        await self.save({
            "program": item_id, "program_execution": "device",
            "scene": None, "preview": None, "restore": None,
        })
        self.sent = None
        self.sent_at = dt_util.utcnow().isoformat()
        self.error = None
        self.hub.log_activity(
            "Tagesprogramm auf Lampe gespeichert",
            f"{program['name']}; 24 Stundenpunkte; lampeneigener Automatikmodus",
        )

    async def command(self, msg: dict) -> None:
        async with self.lock:
            if self.closed:
                raise ValueError("Die Integration wird gerade neu geladen.")
            action = msg["action"]
            kind = msg.get("kind", "programs")
            item_id = msg.get("item_id")
            if self.library is None:
                raise ValueError("Die gemeinsame Bibliothek wird noch geladen.")
            library = self.library.data[kind]
            scene = self.data["scene"]
            if action not in ("list",):
                self.hub.log_activity("AiPai-Studio-Aktion", action)
            in_use = item_id is not None and (
                self.data["program"] == item_id or (scene and scene["id"] == item_id)
            )
            if action in ("save", "delete"):
                # It is safe (and expected) to refine an active program/scene.
                # Persist the new version first and re-evaluate its output below.
                # Deleting the active entry is still prohibited: it would leave
                # the currently controlling schedule without a source.
                if action == "delete" and in_use:
                    raise ValueError("Bitte zuerst das aktive Programm oder die Szene beenden.")
                if item_id is not None and item_id not in library:
                    raise ValueError("Der Eintrag wurde bereits gelöscht.")
                if action == "save":
                    item_id = await self.library.save_item(kind, item_id, msg.get("item", {}))
                elif item_id is not None:
                    # A shared item may be active on a different lamp too.
                    if any(
                        other.data.get("program") == item_id or
                        (other.data.get("scene") and other.data["scene"].get("id") == item_id)
                        for other in (getattr(hub, "studio", None) for hub in self.hub.hass.data.get(DOMAIN, {}).values())
                        if other is not None
                    ):
                        raise ValueError("Bitte zuerst das aktive Programm oder die Szene beenden.")
                    await self.library.delete_item(kind, item_id)
                if action == "save" and kind == "programs":
                    active_device_studios = [
                        other for other in (
                            getattr(hub, "studio", None)
                            for hub in self.hub.hass.data.get(DOMAIN, {}).values()
                        )
                        if other is not None and other.data.get("program") == item_id
                        and other.data.get("program_execution") == "device"
                    ]
                    try:
                        for other in active_device_studios:
                            await other.deploy_program(item_id)
                    except (OSError, ValueError) as err:
                        self.error = str(err)
                        self.hub.log_activity("AiPai-Studio-Ausgabe fehlgeschlagen", str(err), "error")
                        raise
                elif action == "save" and in_use and not self.startup_hold:
                    try:
                        self.sent = None
                        await self.update_output()
                    except (OSError, ValueError) as err:
                        self.error = str(err)
                        self.hub.log_activity("AiPai-Studio-Ausgabe fehlgeschlagen", str(err), "error")
                        raise
                return
            if not self.hub.local:
                raise ValueError("Für die Lampensteuerung bitte Local control aktivieren.")
            if action == "preview":
                if not self.hub.has_state or not self.hub.available:
                    raise OSError("Die Lampe ist gerade nicht erreichbar.")
                levels = validate_levels(msg.get("levels"), self.hub.roads)
                preview_seconds = msg.get("preview_seconds", 5 * 60)
                self.startup_hold = False
                await self.save({
                    "preview": {"levels": levels, "until": dt_util.utcnow().timestamp() + preview_seconds},
                })
                self.sent = None
                try:
                    await self.update_output()
                except (OSError, ValueError) as err:
                    self.error = str(err)
                    self.hub.log_activity("AiPai-Studio-Ausgabe fehlgeschlagen", str(err), "error")
                    raise
                return
            if action == "activate":
                if not self.hub.has_state or not self.hub.available:
                    raise OSError("Die Lampe ist gerade nicht erreichbar.")
                if item_id not in library:
                    raise ValueError("Bitte einen gespeicherten Eintrag wählen.")
                validate_library_item(kind, library[item_id])
                if kind == "programs":
                    await self.deploy_program(item_id)
                    return
                else:
                    self.startup_hold = False
                    until = dt_util.utcnow().timestamp() + 60 * msg.get("duration", 15)
                    await self.save({"scene": {"id": item_id, "until": until}, "preview": None})
            elif action == "manual":
                if kind != "scenes" or item_id not in library:
                    raise ValueError("Bitte eine gespeicherte Szene für den manuellen Modus wählen.")
                validate_library_item("scenes", library[item_id])
                self.startup_hold = False
                await self.save({"scene": {"id": item_id, "until": None, "manual": True}, "preview": None})
            elif action == "preview_cancel":
                # Only stop the temporary view.  Keep the program or scene
                # underneath intact so the normal profile resumes immediately.
                self.startup_hold = False
                await self.save({"preview": None})
            elif action == "resume":
                self.startup_hold = False
                await self.save({"scene": None, "preview": None})
            else:
                self.startup_hold = False
                await self.save({
                    "scene": None, "program": None, "program_execution": None, "preview": None,
                })
            self.sent = None
            try:
                await self.update_output()
            except (OSError, ValueError) as err:
                self.error = str(err)
                self.hub.log_activity("AiPai-Studio-Ausgabe fehlgeschlagen", str(err), "error")
                raise


def register_studio_api(hass) -> None:
    if hass.data.get(f"{DOMAIN}_studio_api"):
        return
    websocket_api.async_register_command(hass, studio_command)
    hass.data[f"{DOMAIN}_studio_api"] = True


@websocket_api.websocket_command({
    vol.Required("type"): "aipai_light/studio",
    vol.Required("action"): vol.In((
        "list", "save", "delete", "activate", "manual", "preview", "preview_cancel", "resume", "release", "group_save", "group_delete",
    )),
    vol.Optional("serial"): str,
    vol.Optional("kind"): vol.In(("programs", "scenes")),
    vol.Optional("item_id"): str,
    vol.Optional("item"): dict,
    vol.Optional("duration"): vol.All(int, vol.Range(min=1, max=60)),
    vol.Optional("preview_seconds"): vol.All(int, vol.Range(min=30, max=60 * 60)),
    vol.Optional("levels"): [int],
    vol.Optional("name"): str,
    vol.Optional("serials"): [str],
})
@websocket_api.async_response
async def studio_command(hass, connection, msg) -> None:
    if not connection.user.is_admin:
        connection.send_error(msg["id"], "unauthorized", "Administratorzugriff erforderlich.")
        return
    studios = {
        hub.serial: hub.studio for hub in hass.data.get(DOMAIN, {}).values()
        if isinstance(hub, AipaiLightHub) and getattr(hub, "studio", None)
    }
    from .groups import async_get_groups

    groups = await async_get_groups(hass)
    from .updates import get_update_status

    update_status = get_update_status(hass)
    try:
        action = msg["action"]
        if action == "group_save":
            await groups.save(msg.get("item_id"), msg.get("name", ""), msg.get("serials", []))
        elif action == "group_delete":
            await groups.delete(msg.get("item_id", ""))
        elif action != "list":
            serial = msg.get("serial", "")
            if serial.startswith("group:"):
                await groups.command(serial.removeprefix("group:"), msg)
            else:
                studio = studios.get(serial)
                if studio is None:
                    raise ValueError("Lampe nicht gefunden.")
                await studio.command(msg)
    except (OSError, ValueError) as err:
        connection.send_error(msg["id"], "studio_error", str(err))
        return
    connection.send_result(
        msg["id"], {
            "lights": [studio.snapshot() for studio in studios.values()],
            "groups": groups.snapshot(studios),
            "update": update_status,
        },
    )
