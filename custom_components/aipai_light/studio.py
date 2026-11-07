"""Minute-resolution lighting and scenes for the native Reef Studio panel."""
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


def levels_at(points: list[dict], minute: int, curve: str = "linear") -> list[int]:
    """Interpolate across midnight as well as between daytime points."""
    right = next((i for i, p in enumerate(points) if p["minute"] > minute), 0)
    start, end = points[right - 1], points[right]
    span = (end["minute"] - start["minute"]) % DAY
    fraction = ((minute - start["minute"]) % DAY) / span
    if curve == "sine":
        from math import cos, pi

        fraction = (1 - cos(pi * fraction)) / 2
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
        curve = point.get("curve", "linear")
        if curve not in ("linear", "sine"):
            raise ValueError("Kurvenform muss linear oder sinus sein.")
        validated.append({
            "minute": minute, "levels": validate_levels(point.get("levels"), count), "curve": curve,
        })
    if len({point["minute"] for point in validated}) != len(validated):
        raise ValueError("Jede Uhrzeit darf nur einmal vorkommen.")
    # Older saved programs stored a curve per point. Keep them compatible by
    # promoting any prior sine selection to one global program curve.
    curve = item.get("curve")
    if curve is None:
        curve = "sine" if any(point["curve"] == "sine" for point in validated) else "linear"
    if curve not in ("linear", "sine"):
        raise ValueError("Kurvenform muss linear oder sinus sein.")
    return {
        "name": name.strip(), "points": sorted(validated, key=lambda point: point["minute"]),
        "curve": curve,
    }


class ReefStudio:
    """Persistent library and a serialized controller for one local fixture."""

    def __init__(self, hub: AipaiLightHub) -> None:
        self.hub = hub
        self.store = Store(hub.hass, 1, f"{DOMAIN}.studio.{hub.serial}")
        self.data = {
            "programs": {}, "scenes": {}, "program": None, "scene": None,
            "preview": None, "restore": None,
        }
        self.lock = asyncio.Lock()
        self.sent: list[int] | None = None
        self.sent_at: str | None = None
        self.error: str | None = None
        self.unsubscribe = None
        self.expiry_cancel = None
        self.closed = False

    async def start(self) -> None:
        self.data.update(await self.store.async_load() or {})
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
            **{key: self.data[key] for key in ("programs", "scenes", "program", "scene", "preview")},
            "serial": self.hub.serial,
            "name": self.hub.name,
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
        return any(self.data[key] for key in ("program", "scene", "preview", "restore"))

    async def tick(self, _now=None) -> None:
        if self.closed or self.lock.locked():
            return
        async with self.lock:
            try:
                # The fixture's local HTTP channel can drop a static manual
                # level after a few minutes. Re-send studio-controlled output
                # at every minute boundary, even on a flat program plateau.
                if any(self.data[key] for key in ("program", "scene", "preview")):
                    self.sent = None
                await self.update_output()
            except (OSError, ValueError) as err:
                self.sent = None
                self.error = str(err)
                self.hub.log_activity("Reef-Studio-Ausgabe fehlgeschlagen", str(err), "error")

    async def update_output(self) -> None:
        preview = self.data["preview"]
        if preview and preview["until"] <= dt_util.utcnow().timestamp():
            await self.save({"preview": None})
            preview = None
        scene = self.data["scene"]
        if scene and scene.get("until") is not None and scene["until"] <= dt_util.utcnow().timestamp():
            await self.save({"scene": None})
            scene = None
        program = self.data["program"]
        restore = self.data["restore"]
        if not self.hub.local or not (program or scene or preview or restore):
            return
        if not self.hub.has_state:
            raise OSError("Warte auf den ersten Status der Lampe.")
        target = None
        if preview:
            target = validate_levels(preview["levels"], self.hub.roads)
        elif scene:
            target = validate_levels(self.data["scenes"][scene["id"]]["levels"], self.hub.roads)
        elif program:
            now = self.hub._now()
            validated = validate_item("programs", self.data["programs"][program], self.hub.roads)
            target = levels_at(validated["points"], now.hour * 60 + now.minute, validated["curve"])

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

    async def command(self, msg: dict) -> None:
        async with self.lock:
            if self.closed:
                raise ValueError("Die Integration wird gerade neu geladen.")
            action = msg["action"]
            kind = msg.get("kind", "programs")
            item_id = msg.get("item_id")
            library = self.data[kind]
            scene = self.data["scene"]
            if action not in ("list",):
                self.hub.log_activity("Reef-Studio-Aktion", action)
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
                updated = dict(library)
                if action == "save":
                    if len(library) >= 100 and item_id is None:
                        raise ValueError("Maximal 100 Einträge pro Bibliothek.")
                    updated[item_id or uuid4().hex] = validate_item(kind, msg.get("item", {}), self.hub.roads)
                elif item_id is not None:
                    del updated[item_id]
                await self.save({kind: updated})
                if action == "save" and in_use:
                    self.sent = None
                    try:
                        await self.update_output()
                    except (OSError, ValueError) as err:
                        self.error = str(err)
                        self.hub.log_activity("Reef-Studio-Ausgabe fehlgeschlagen", str(err), "error")
                        raise
                return
            if not self.hub.local:
                raise ValueError("Für die Lampensteuerung bitte Local control aktivieren.")
            if action == "preview":
                if not self.hub.has_state or not self.hub.available:
                    raise OSError("Die Lampe ist gerade nicht erreichbar.")
                levels = validate_levels(msg.get("levels"), self.hub.roads)
                preview_seconds = msg.get("preview_seconds", 5 * 60)
                await self.save({
                    "preview": {"levels": levels, "until": dt_util.utcnow().timestamp() + preview_seconds},
                })
                self.sent = None
                try:
                    await self.update_output()
                except (OSError, ValueError) as err:
                    self.error = str(err)
                    self.hub.log_activity("Reef-Studio-Ausgabe fehlgeschlagen", str(err), "error")
                    raise
                return
            if action == "activate":
                if not self.hub.has_state or not self.hub.available:
                    raise OSError("Die Lampe ist gerade nicht erreichbar.")
                if item_id not in library:
                    raise ValueError("Bitte einen gespeicherten Eintrag wählen.")
                validate_item(kind, library[item_id], self.hub.roads)
                if kind == "programs":
                    await self.save({"program": item_id, "scene": None, "preview": None})
                else:
                    until = dt_util.utcnow().timestamp() + 60 * msg.get("duration", 15)
                    await self.save({"scene": {"id": item_id, "until": until}, "preview": None})
            elif action == "manual":
                if kind != "scenes" or item_id not in library:
                    raise ValueError("Bitte eine gespeicherte Szene für den manuellen Modus wählen.")
                validate_item("scenes", library[item_id], self.hub.roads)
                await self.save({"scene": {"id": item_id, "until": None, "manual": True}, "preview": None})
            elif action == "preview_cancel":
                # Only stop the temporary view.  Keep the program or scene
                # underneath intact so the normal profile resumes immediately.
                await self.save({"preview": None})
            elif action == "resume":
                await self.save({"scene": None, "preview": None})
            else:
                await self.save({"scene": None, "program": None, "preview": None})
            self.sent = None
            try:
                await self.update_output()
            except (OSError, ValueError) as err:
                self.error = str(err)
                self.hub.log_activity("Reef-Studio-Ausgabe fehlgeschlagen", str(err), "error")
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
                await groups.command(
                    serial.removeprefix("group:"), action, msg.get("kind", "programs"),
                    msg.get("item_id"), msg.get("duration", 15),
                )
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
