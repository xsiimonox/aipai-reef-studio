"""Persistent lamp groups for Reef Studio."""
from __future__ import annotations

from uuid import uuid4

from homeassistant.helpers.storage import Store

from .const import DOMAIN


class ReefGroups:
    """Groups reference fixtures and can control their shared Studio library."""

    def __init__(self, hass) -> None:  # noqa: ANN001
        self.hass = hass
        self.store = Store(hass, 1, f"{DOMAIN}.groups")
        self.groups: dict[str, dict] = {}

    async def load(self) -> None:
        data = await self.store.async_load() or {}
        self.groups = data.get("groups", {}) if isinstance(data.get("groups"), dict) else {}

    async def save(self, group_id: str | None, name: str, serials: list[str]) -> None:
        name = name.strip()
        if not 1 <= len(name) <= 64:
            raise ValueError("Bitte einen Gruppennamen mit 1 bis 64 Zeichen eingeben.")
        if not serials:
            raise ValueError("Bitte mindestens eine Lampe für die Gruppe auswählen.")
        if len(set(serials)) != len(serials):
            raise ValueError("Eine Lampe kann nur einmal in derselben Gruppe enthalten sein.")
        hubs = self.hass.data.get(DOMAIN, {})
        if any(serial not in {hub.serial for hub in hubs.values()} for serial in serials):
            raise ValueError("Eine ausgewählte Lampe ist nicht mehr verfügbar.")
        if group_id and group_id not in self.groups:
            raise ValueError("Die Gruppe wurde bereits gelöscht.")
        self.groups[group_id or uuid4().hex] = {"name": name, "serials": serials}
        await self.store.async_save({"groups": self.groups})

    async def delete(self, group_id: str) -> None:
        if group_id not in self.groups:
            raise ValueError("Die Gruppe wurde bereits gelöscht.")
        del self.groups[group_id]
        await self.store.async_save({"groups": self.groups})

    def snapshot(self, studios: dict) -> list[dict]:
        output = []
        for group_id, group in self.groups.items():
            members = [studios[serial] for serial in group["serials"] if serial in studios]
            if not members or len(members) != len(group["serials"]):
                continue
            # Channel order is common across A7/A8SE and MAX models. Expose
            # the largest profile; each controller fits output to its fixture.
            group_labels = list(max((studio.hub.labels for studio in members), key=len))
            def shared(kind: str) -> dict:
                # Programmes and scenes are now stored once for all fixtures.
                # Returning their original ids makes the group editor fully
                # equivalent to the editor for one selected lamp.
                library = members[0].library
                return dict(library.data[kind]) if library is not None else {}
            program = members[0].data.get("program")
            if not all(studio.data.get("program") == program for studio in members):
                program = None
            scene = members[0].data.get("scene")
            if scene and not all(
                other.data.get("scene") and other.data["scene"].get("id") == scene.get("id")
                for other in members
            ):
                scene = None
            preview = members[0].data.get("preview")
            if preview and not all(
                other.data.get("preview") and
                other.data["preview"].get("levels", [])[:min(len(other.hub.labels), len(members[0].hub.labels))] ==
                preview.get("levels", [])[:min(len(other.hub.labels), len(members[0].hub.labels))]
                for other in members
            ):
                preview = None
            output.append({
                "serial": f"group:{group_id}", "group_id": group_id, "name": group["name"],
                "members": group["serials"], "labels": group_labels,
                "local": all(studio.hub.local for studio in members),
                "online": all(studio.hub.available for studio in members),
                "minute": members[0].hub._now().hour * 60 + members[0].hub._now().minute,
                "timezone": str(members[0].hub.timezone), "reported": [], "firmware_points": [],
                "programs": shared("programs"), "scenes": shared("scenes"), "program": program,
                "scene": scene, "preview": preview, "sent": None, "sent_at": None, "error": None, "group": True,
            })
        return output

    async def command(self, group_id: str, msg: dict) -> None:
        group = self.groups.get(group_id)
        if group is None:
            raise ValueError("Gruppe nicht gefunden.")
        studios = {hub.serial: hub.studio for hub in self.hass.data.get(DOMAIN, {}).values()
                   if getattr(hub, "studio", None)}
        members = [studios.get(serial) for serial in group["serials"]]
        if any(member is None for member in members):
            raise ValueError("Nicht alle Gruppenlampen sind geladen.")
        action = msg["action"]
        # Library changes never talk to the fixture. One member is enough,
        # because every member uses the same persistent library.
        if action in ("save", "delete"):
            await members[0].command(msg)
            return
        for studio in members:
            if not studio.hub.local or not studio.hub.available:
                raise OSError(f"{studio.hub.display_name} ist nicht lokal erreichbar.")
        for studio in members:
            command = dict(msg)
            if action == "preview":
                # Mixed groups expose the largest profile. Truncate only this
                # fixture's outgoing values, retaining UV/warm white for MAX.
                levels = command.get("levels", [])
                command["levels"] = (levels + [0] * studio.hub.roads)[:studio.hub.roads]
            await studio.command(command)


async def async_get_groups(hass) -> ReefGroups:  # noqa: ANN001
    key = f"{DOMAIN}_groups"
    groups = hass.data.get(key)
    if groups is None:
        groups = ReefGroups(hass)
        await groups.load()
        hass.data[key] = groups
    return groups
