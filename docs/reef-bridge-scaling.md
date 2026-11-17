# Reef Bridge: Größe und Skalierung

## Eine Bridge

Die ESP32-WROOM-Firmware verwaltet bis zu zwölf AIPAI-Lampen. Acht oder weniger Lampen pro Bridge sind die bessere Wahl, wenn Gruppen oft live angesehen oder Szenen häufig umgeschaltet werden: Die lokale HTTP-Schnittstelle der Lampen verarbeitet Befehle nacheinander, nicht als Broadcast.

Tagesprogramme sind davon kaum betroffen. Sie werden dauerhaft in den Lampen gespeichert und laufen dort ohne Bridge, Home Assistant oder Internet weiter.

## 30 oder mehr Lampen

Mehrere Bridges sind zuverlässiger als ein einzelner überladener ESP32. Als Ausgangspunkt:

| Lampenzahl | Empfohlener Aufbau |
| --- | --- |
| 1–8 | Eine Reef Bridge |
| 9–12 | Eine Bridge, wenn schnelle Gruppeninteraktion nicht im Vordergrund steht |
| 13–24 | Zwei Bridges mit je 6–10 Lampen |
| 25–36 | Drei Bridges mit je 8–10 Lampen |

Jede Bridge braucht eine feste DHCP-Zuordnung und sollte im gleichen vertrauenswürdigen 2,4-GHz-Netz wie ihre Lampen arbeiten. Bei vielen Geräten sind mehrere Access Points oder ein gut dimensioniertes 2,4-GHz-WLAN wichtiger als ein leistungsstärkerer ESP.

AiPai-Studio-Gruppen gehören jeweils zu genau einer Bridge. Bridgeübergreifende Szenen können aktuell mit individuellen REST-Aufrufen je Bridge in einer Home-Assistant-Automation parallel ausgelöst werden. Eine zentrale Mehr-Bridge-Orchestrierung ist nicht Bestandteil der Bridge-Firmware.

## Unterstützte Boards

Das veröffentlichte Abbild ist für ESP32-WROOM-32/ESP32 DevKit mit mindestens 4 MB Flash gebaut. ESP8266, NodeMCU/D1 mini sowie ESP32-C3/S2/S3 werden nicht unterstützt. Mehr Flash oder PSRAM ersetzt keine WLAN-Kapazität der Lampen und erhöht die getestete Zwölf-Lampen-Grenze nicht automatisch.
