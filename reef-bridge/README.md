# AIPAI Reef Bridge für ESP32

Firmware **0.2.0** mit demselben Reef-Studio-Editor wie Home Assistant. Tagesprogramme, Szenen und Gruppen laufen lokal auf dem ESP32. Home Assistant ist optional. Experimenteller Stand: Builds und simulierte Browsertests sind geprüft; der Betrieb mit mehreren echten Lampen benötigt noch einen Hardware-Dauertest.

![Reef Studio auf der Bridge](docs/bridge-desktop.png)

<details><summary>Mobile Ansicht und Architektur</summary>

<img src="docs/bridge-mobile.png" width="300" alt="Reef Studio auf dem Smartphone">

![Architektur und HA-Fallback](docs/architecture.svg)

</details>

## Gemeinsame Bedienung

- Identischer Editor und identisches Design mit der HA-Integration: grafischer Tagesverlauf, Stützpunkte mit Uhrzeit, lineare Übergänge, Rampen-Assistent und proportionaler Gesamtintensitätsregler.
- Gemeinsame lokale Programm- und Szenenbibliothek für alle Lampen dieser Bridge. Neue Einträge, Bearbeiten, Duplizieren, Löschen und Aktivieren. Bis 32 Programme und 32 Szenen, insgesamt maximal 48 KB JSON bzw. die verfügbare Parser-Kapazität; größere Änderungen werden mit Fehlermeldung abgelehnt.
- Bis zu vier Lampen im selben Netz; jeweils Name, IP, Seriennummer, 6/8 Kanäle und POSIX-Zeitzone. Bis 16 Gruppen, auch mit nur einer Lampe. Die Gruppe zeigt die gemeinsam vorhandenen Kanäle; MAX-Profile bleiben beim Bearbeiten an Sechskanal-Lampen vollständig erhalten.
- Programme laufen lokal, ohne offenen Browser oder Home Assistant. Jede Lampe hat einen eigenen aktiven Eintrag und eigene Timer. Ein Profilwechsel einer Lampe ändert nicht automatisch die Auswahl einer anderen Lampe.
- Temporäre Szenen 1–60 Minuten, dauerhafter manueller Modus, Countdown und Abbruch. Regler einer einzelnen Lampe starten eine Vorschau für eine Minute im Programmeditor bzw. fünf Minuten im Szeneneditor. Gruppen lassen sich über die Uhrzeit-Vorschau oder eine gespeicherte Szene ansehen.
- Temperatur, aktive Lichtart, Update-Hinweis, Diagnoseprotokoll und vollständiges JSON-Backup/Import. Die Bridge prüft Firmware-Updates nach WLAN-Verbindung und alle sechs Stunden. Installation bewusst über den USB-Installer; OTA ist noch nicht implementiert.
- Beim Neustart bleiben Bibliothek, Lampen, Gruppen und aktive Tagesprogramme gespeichert. Szenen, manuelle Eingriffe und Vorschauen enden; nach gültiger NTP-Zeit übernimmt das lokale Tagesprofil.

„Synchron gehalten“ bedeutet: Beide Pakete bauen den Editor direkt aus `custom_components/aipai_light/panel/reef-studio.js`. Der Adapter enthält nur Bridge-Transport und Hardwareeinrichtung. Es gibt keine zweite Kopie der Layouts oder Editoren. CI prüft die Studio-Version und baut die Bridge auch bei Änderungen am gemeinsamen Panel. Bereits geflashte Geräte erhalten neue Funktionen erst mit einem Firmware-Update. Die Bibliotheken einer HA-Installation und einer Bridge werden **nicht automatisch über das Netz miteinander abgeglichen**.

## Hardware und Installation

Klassischer **ESP32-WROOM-32 / ESP32 DevKit (esp32dev), mindestens 4 MB Flash**. ESP8266/NodeMCU/D1 mini und ESP32-C3/S2/S3 werden vom bereitgestellten Abbild nicht unterstützt. Keine Pins an die Lampe anschließen; Bridge und Lampen sprechen über das 2,4-GHz-WLAN.

1. [Browser-Installer](https://xsiimonox.github.io/aipai-reef-studio/) in Chrome oder Edge auf Windows/macOS öffnen und den ESP32 mit einem USB-Datenkabel verbinden. Safari/Firefox unterstützen Web Serial nicht. Bei Bedarf CP210x-/CH340-Treiber installieren und BOOT beim Verbinden gedrückt halten.
2. Firmware installieren. Vor einem Update Backup herunterladen; „Erase device“ löscht auch WLAN und Einstellungen.
3. Mit `ReefBridge-…` verbinden. Bei neuer Installation lautet das WLAN-Passwort `ReefBridge2026!`. Frühere Installationen behalten ihr Passwort; in der USB-Konsole bei 115200 Baud `INFO` senden, um es und die IP zu sehen.
4. Das Captive Portal öffnet normalerweise direkt. Andernfalls **http://192.168.4.1** aufrufen. Kein API-Token nötig.
5. Unter „Einrichtung“ das Heim-WLAN speichern. Nach Verbindung endet das Setup-WLAN. Die neue Bridge-IP im Router öffnen; alternativ funktioniert im Heimnetz auf unterstützten Clients `http://reefbridge.local`.
6. Lampen unter „Einrichtung“ hinzufügen: IP, Seriennummer, Name, Kanalprofil und Zeitzone. Feste DHCP-Zuordnungen im Router verwenden. Europa/Berlin ist inklusive Sommerzeit als POSIX-Zeitzone vorbelegt.
7. Tagesprogramm grafisch erstellen, speichern und für Lampe oder Gruppe aktivieren. Neu hinzugefügte Lampen verwenden bis zur ersten Programmaktivierung ein ausgeschaltetes Fallback-Profil. Vor Betrieb die gewünschten Kanalwerte prüfen.

Beim ersten Start ab 0.2.0 wird eine bestehende 0.1-Konfiguration mitsamt Tagesplan und Szenen automatisch übernommen. Die alte Preferences-Konfiguration bleibt als Wiederherstellungskopie erhalten. Neue Daten werden in LittleFS über eine temporäre Datei gespeichert. Ein Mount-Fehler nach der ersten Initialisierung führt nicht zu automatischem Formatieren.

Terminal-Alternative: Unter GitHub Actions → „Reef Bridge firmware and installer“ das Artefakt `reef-bridge-esp32-factory` herunterladen/entpacken. Windows: `py -m pip install esptool==4.8.1`, danach `py flash.py --port COM3 --firmware firmware.bin`. macOS: virtuelle Python-Umgebung anlegen, `pip install esptool==4.8.1`, danach `python flash.py --port /dev/cu.usbserial-0001 --firmware firmware.bin`. Port anpassen. Das Factory-Abbild enthält Bootloader und Partitionstabelle.

## Zeit und Ausfälle

Nach jedem Kaltstart wird eine gültige NTP-Zeit benötigt, standardmäßig mit Internet. Ohne gültige Zeit schreibt die Bridge keinen zeitabhängigen Tagesplan. Einmal synchronisiert läuft die Uhr bis zum Neustart lokal weiter. Lokaler NTP-Server/RTC-Konfiguration ist noch nicht enthalten.

Die Bridge sendet Zielwerte alle 30 Sekunden erneut. Netzwerktransaktionen zu den Lampen laufen nacheinander, nicht gleichzeitig; HTTP-Laufzeiten können den tatsächlichen Lichtwechsel verzögern. Timer werden unabhängig je Lampe verwaltet. Lüftergrenzen und lampeneigene Stundenkurven werden bei Moduswechseln erhalten. Seriennummer und Protokollformat werden vor jedem Schreibzyklus überprüft.

Beim Ausfall von HA läuft die Bridge weiter. Beim Ausfall der Bridge selbst gibt es **keinen garantierten automatischen Wechsel zum lampeneigenen Tagesplan**, da die Lampen für die feine Steuerung im manuellen Firmwaremodus stehen. „Lampeneigenen Modus wiederherstellen“ wechselt bewusst zurück zur Stundenkurve der Lampe. Nach Bridge-Neustart übernimmt wieder das lokale Bridge-Tagesprofil. Hersteller-App oder direkt schreibende HA-Integration während des Bridge-Betriebs nicht parallel zum Steuern verwenden.

## Optionale Home-Assistant-Anbindung

HA → Bridge → Lampe. Der bestehende REST-Lease-Einstieg steuert die **erste eingerichtete Lampe**; weitere Lampen und Gruppen sind über die Studio-API verfügbar.

1. [home-assistant/reef_bridge.yaml](home-assistant/reef_bridge.yaml) nach `/config/packages/reef_bridge.yaml` kopieren.
2. Unter dem bestehenden `homeassistant:`-Abschnitt in `configuration.yaml` `packages: !include_dir_named packages` aktivieren.
3. `secrets.yaml` ergänzen:

```yaml
reef_bridge_status_url: http://192.168.178.50/api/status
reef_bridge_control_url: http://192.168.178.50/api/control
```

4. HA-Konfiguration prüfen und neu starten. „Reef Bridge durch HA steuern“ sendet alle 30 Sekunden eine Lease für 90 Sekunden. Bei acht Kanälen die Helferliste auf acht Zahlen erweitern. Ausschalten kehrt zum lokalen Tagesplan zurück. Bei HA-Ausfall endet die Lease nach 90 Sekunden; HTTP-Laufzeit kann den Lichtwechsel verzögern.

Der dauerhafte manuelle Modus lehnt HA-Leases mit HTTP 409 ab. Update-Status steht im Attribut `update` des Modussensors. Keine Cloudanmeldung, kein MQTT nötig. REST ist tokenfrei und ausschließlich für ein vertrauenswürdiges lokales Netz vorgesehen, ohne Internet-Portfreigabe.

## Lokale API

| Methode / Pfad | Funktion |
| --- | --- |
| GET `/api/status` | `workspace` mit Lampen/Bibliothek/Gruppen; `lights` mit Laufzeitstatus; Firmware, Update und kompatibler REST-Status der ersten Lampe |
| POST `/api/studio` | Gleiche Aktionen wie Reef Studio: `save`, `delete`, `activate`, `manual`, `preview`, `preview_cancel`, `resume`, `release`, `group_save`, `group_delete` |
| POST `/api/fixture` | `index` (-1 für neu), `name`, `lamp_ip`, `serial`, `channels`, `timezone` |
| POST `/api/restore` | Vollständiges `workspace`-Backup importieren; alle Overrides beenden |
| POST `/api/wifi` | `ssid`, `password` |
| POST `/api/control` | Kompatible erste-Lampe-Aktionen `lease`, `preview`, `manual`, `resume` mit `levels` und `seconds` |

Studio-Beispiel: `{"action":"activate","serial":"SERIENNUMMER","kind":"programs","item_id":"PROFIL_ID"}`. Gruppen verwenden `serial: "group:GRUPPEN_ID"`. Vorschauen erhalten `preview_seconds` (1–3600); Szenen `duration` (1–60 Minuten). Das frühere `/api/config` wird durch `/api/fixture`, `/api/studio` und `/api/restore` ersetzt. Alte Backups können bei der Migration aus der bestehenden 0.1-Installation übernommen werden; der neue Import erwartet das vollständige Workspace-Format.

## Entwicklung und gemeinsame Releases

```sh
python3 tools/validate_release.py
python3 tools/validate_studio_sync.py
pio run -d reef-bridge
node tools/preview_studio.cjs
node reef-bridge/tools/preview.cjs
```

Die Vorschauen benötigen Playwright/Chromium und verwenden ausschließlich simulierte API-Antworten. Bei Bedarf `PLAYWRIGHT_MODULE` auf die lokale Playwright-Installation setzen. Die Bridge-Vorschau schreibt Bilder nach `/tmp/reef-bridge-studio` (überschreibbar mit `BRIDGE_SCREENSHOT_DIR`).

Bei gemeinsamen Änderungen HA-Version, Panel-Cache, Bridge-Firmware-Version, Installer und `studio_version` im Update-Manifest gemeinsam pflegen, siehe [AGENTS.md](../AGENTS.md). GitHub Actions validiert, baut die Firmware und veröffentlicht Installer/Update-Manifest auf Pages. [Bridge-Changelog](CHANGELOG.md).

Vor regulärem Aquarienbetrieb am echten ESP32 testen: Migration und Stromausfall beim Speichern, Neustart, 6/8-Kanal-Mischgruppen, Timer, manuell, NTP/Zeitzonen, Seriennummer-Sperre, WLAN-/HA-Ausfall und mindestens 24 Stunden Dauerbetrieb. Keine automatisierten Schreibtests am realen Aquarium.

MIT, siehe [LICENSE](../LICENSE). Unabhängiges Community-Projekt ohne Herstellerzugehörigkeit.
