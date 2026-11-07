"""Persistent lamp groups for Reef Studio."""
from __future__ import annotations

from uuid import uuid4

from homeassistant.helpers.storage import Store

from .const import DOMAIN


class ReefGroups:
    """Groups reference fixture serials and synchronize same-named studio items."""

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
            if len(members) != len(group["serials"]):
                continue
            shared_labels = [
                label for label in members[0].hub.labels
                if all(label in studio.hub.labels for studio in members[1:])
            ]
            def shared(kind: str) -> dict:
                names = {
                    item["name"] for item in members[0].data[kind].values()
                    if all(any(other["name"] == item["name"] for other in studio.data[kind].values())
                           for studio in members[1:])
                }
                return {name: {"name": name} for name in sorted(names)}
            output.append({
                "serial": f"group:{group_id}", "group_id": group_id, "name": group["name"],
                "members": group["serials"], "labels": shared_labels,
                "local": all(studio.hub.local for studio in members),
                "online": all(studio.hub.available for studio in members),
                "minute": members[0].hub._now().hour * 60 + members[0].hub._now().minute,
                "timezone": str(members[0].hub.timezone), "reported": [], "firmware_points": [],
                "programs": shared("programs"), "scenes": shared("scenes"), "program": None,
                "scene": None, "sent": None, "sent_at": None, "error": None, "group": True,
            })
        return output

    async def command(self, group_id: str, action: str, kind: str, item_name: str | None, duration: int) -> None:
        group = self.groups.get(group_id)
        if group is None:
            raise ValueError("Gruppe nicht gefunden.")
        studios = {hub.serial: hub.studio for hub in self.hass.data.get(DOMAIN, {}).values()
                   if getattr(hub, "studio", None)}
        members = [studios.get(serial) for serial in group["serials"]]
        if any(member is None for member in members):
            raise ValueError("Nicht alle Gruppenlampen sind geladen.")
        for studio in members:
            if not studio.hub.local or not studio.hub.available:
                raise OSError(f"{studio.hub.display_name} ist nicht lokal erreichbar.")
        if action in ("activate",):
            matches = []
            for studio in members:
                match = next((item_id for item_id, item in studio.data[kind].items()
                              if item["name"] == item_name), None)
                if match is None:
                    raise ValueError(f"{studio.hub.display_name} hat keinen Eintrag „{item_name}“.")
                matches.append(match)
            for studio, item_id in zip(members, matches, strict=True):
                await studio.command({"action": action, "kind": kind, "item_id": item_id,
                                      "duration": duration})
            return
        for studio in members:
            await studio.command({"action": action, "kind": kind})


async def async_get_groups(hass) -> ReefGroups:  # noqa: ANN001
    key = f"{DOMAIN}_groups"
    groups = hass.data.get(key)
    if groups is None:
        groups = ReefGroups(hass)
        await groups.load()
        hass.data[key] = groups
    return groups
