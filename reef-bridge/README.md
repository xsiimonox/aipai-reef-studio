# AIPAI Reef Bridge für ESP32

Eigenständige lokale Steuerung für AIPAI-LED-Lampen – ohne Home Assistant erforderlich. Optional kann Home Assistant über die lokale Bridge-API steuern. Dies ist **Version 0.1.0, ein experimenteller Prototyp**. Kompilieren und simulierte Prüfungen ersetzen keinen Dauertest am echten ESP32 und der Lampe.

![Architektur und Fallback](docs/architecture.svg)

<details><summary>Oberfläche ansehen (echte Browseraufnahme mit simulierten Beispieldaten)</summary>

![Bridge Desktop-Oberfläche mit Tagesprofil, Spektrum und Einrichtung](docs/bridge-desktop.png)

<img src="docs/bridge-mobile.png" width="300" alt="Mobile Ansicht der Reef Bridge">

</details>

## Was die erste Version kann

- Eine Lampe pro Bridge, sechs oder acht physische Kanäle.
- Ein lokal gespeichertes Tagesprogramm mit 2–48 Zeitpunkten und linearen Übergängen, auch über Mitternacht.
- Bis zu 16 gespeicherte Szenen, Regler-Vorschau, zeitbegrenzte Anzeige für 1–60 Minuten, dauerhafter manueller Modus und sofortige Rückkehr zum Tagesprofil.
- Gerätename/IP, Kanalprofil, Seriennummer-Prüfung, POSIX-Zeitzone mit Sommerzeit, NTP, Temperatur und Diagnoseprotokoll.
- Programme und WLAN bleiben im Flash; nach Neustart läuft das Tagesprogramm weiter, sobald eine gültige Uhrzeit vorliegt. Temporäre/manuelle Overrides werden beim Neustart aufgehoben.
- Weboberfläche liegt vollständig in der Firmware; Backup/Import als JSON.
- Home Assistant steuert über eine Lease: spätestens 90 Sekunden nach der letzten Bestätigung übernimmt das lokale Tagesprogramm.

Mehrere Programmbibliotheken, Lampengruppen, OTA, automatische Suche, HA-Autodiscovery und die vollständige Reef-Studio-HA-Oberfläche sind noch **nicht** enthalten. Die neue Bridge-Oberfläche ist bewusst ein eigener erster Implementierungsstand.

## Hardware

Für das bereitgestellte Firmware-Abbild: **klassischer ESP32-WROOM-32 / ESP32 DevKit (esp32dev), mindestens 4 MB Flash**, USB-Datenkabel und USB-Netzteil. Keine Verbindung mit der Elektronik der Lampe: ESP und Lampe kommunizieren im selben 2,4-GHz-WLAN. Keine Pins an die Lampe anschließen.

ESP32-C3, S2, S3 und andere Flashgrößen brauchen separate Board-Konfigurationen und Firmware-Abbilder. Das klassische ESP32-Abbild ist dafür ungeeignet. ESP8266/D1 mini wird hier nicht unterstützt.

## Flashen auf Windows und macOS

Der [Browser-Installer](https://xsiimonox.github.io/aipai-reef-studio/) wird aus GitHub Actions bereitgestellt. Chrome oder Edge und eine HTTPS-Seite sind erforderlich. Safari/Firefox können Web Serial nicht verwenden. Auf dem Computer ist keine Python-Installation notwendig.

1. ESP32 per **Datenkabel** verbinden. Bei fehlendem USB-Port eventuell den passenden CP210x-/CH340-Treiber des Boardherstellers installieren.
2. Im Browser-Installer „Connect“ / „Install“ wählen und den USB-Port auswählen. Nur das Abbild für klassischen ESP32 verwenden.
3. Bei Verbindungsproblemen BOOT während des Verbindungsaufbaus halten, gegebenenfalls EN/RESET kurz drücken.
4. Nach Installation die Konsole mit 115200 Baud öffnen. `INFO` und Enter senden. Die Bridge gibt Setup-WLAN-Passwort und API-Token aus. Beide lokal aufbewahren und nicht als Screenshot veröffentlichen.
5. Mit dem angezeigten `ReefBridge-…`-WLAN verbinden. `http://192.168.4.1` öffnen und den API-Token eintragen.
6. Heim-WLAN unter „WLAN einrichten“ speichern. Nach Verbindung schließt das Setup-WLAN. Bridge-IP im Router suchen oder `http://reefbridge.local` versuchen. Bei mehreren Bridges die IP verwenden.
7. Lampen-IP und **korrekte Seriennummer** eintragen. Kanalprofil wählen, Tagespunkte festlegen und speichern. Der Controller prüft vor jedem Schreibzyklus die Seriennummer.

Das Setup-WLAN ist nach Neustart zunächst verfügbar und endet bei erfolgreicher WLAN-Verbindung. Bei falschen WLAN-Daten bleibt es erreichbar; so lässt sich die Konfiguration korrigieren. Die USB-Konsole liefert Zugangsdaten bei Bedarf erneut über `INFO`.

### Terminal-Installer als Alternative

Unter GitHub → Actions → „Reef Bridge firmware and installer“ den erfolgreichen Build öffnen und `reef-bridge-esp32-factory` herunterladen/entpacken. Es enthält `firmware.bin` und `flash.py`. Das ist ein zusammengefügtes Factory-Abbild inklusive Bootloader und Partitionstabelle.

Windows PowerShell:

```powershell
py -m pip install esptool==4.8.1
py flash.py --port COM3 --firmware firmware.bin
```

macOS Terminal:

```sh
python3 -m venv .flash-venv
source .flash-venv/bin/activate
python -m pip install esptool==4.8.1
ls /dev/cu.*
python flash.py --port /dev/cu.usbserial-0001 --firmware firmware.bin
```

Port durch den tatsächlichen Gerätenamen ersetzen. Flashen eines Factory-Abbilds kann die Installation überschreiben; vor Updates ein Backup exportieren. Der Browser bietet für Neuinstallationen das vollständige Löschen an, dabei gehen auch WLAN-Daten verloren.

## Zeit und Verhalten bei Ausfällen

Der ESP32 benötigt nach jedem Kaltstart einmal eine gültige NTP-Zeit. Dafür ist standardmäßig Internet nötig. Ohne gültige Uhrzeit schreibt er **kein Tagesprogramm** an die Lampe. Ist die Zeit einmal synchronisiert, läuft die lokale Uhr ohne Internet weiter, bis Strom/Neustart sie zurücksetzt. Komplett netzunabhängiger Kaltstart erfordert später einen lokalen NTP-Server oder RTC-Erweiterung.

Die Bridge sendet alle 30 Sekunden Zielwerte erneut. HTTP-Fehler erscheinen im Diagnosebereich. Bei fehlendem WLAN oder ausgeschaltetem ESP32 kann die Bridge die Lampe nicht steuern; es gibt keine garantierte Rückkehr zu einem sicheren Zustand in der Lampenfirmware. Der ESP32 muss für zeitgesteuerte Szenen dauerhaft laufen. Lüftergrenzen und Firmware-Tagesdaten werden bei einem nötigen Wechsel in den manuellen Firmwaremodus übernommen.

„Tagesprofil fortsetzen“ meint das **auf der Bridge gespeicherte** Programm. Eine Hersteller-App oder eine zweite direkt schreibende HA-Integration kann weiterhin konkurrieren. Für Bridge-Betrieb die bisherige direkte AIPAI-Steuerung deaktivieren.

## Home Assistant optional einbinden

Empfohlener Aufbau: HA → Bridge → Lampe. Die Bridge bleibt jederzeit der einzige direkte Schreiber. HA-Ausfall wird über eine befristete Lease erkannt, nicht durch unzuverlässiges Ping.

1. Inhalt von [home-assistant/reef_bridge.yaml](home-assistant/reef_bridge.yaml) nach `/config/packages/reef_bridge.yaml` kopieren.
2. In `configuration.yaml` unter dem bestehenden `homeassistant:`-Abschnitt aktivieren:

```yaml
homeassistant:
  packages: !include_dir_named packages
```

3. In `secrets.yaml` eintragen (IP und Token ersetzen):

```yaml
reef_bridge_status_url: http://192.168.178.50/api/status
reef_bridge_control_url: http://192.168.178.50/api/control
reef_bridge_authorization: 'Bearer HIER_DEN_API_TOKEN_EINTRAGEN'
```

4. HA-Konfiguration prüfen, neu starten. Modussensor und Helfer erscheinen. Bei acht Kanälen die HA-Spektrum-Liste auf acht Zahlen erweitern.
5. „Reef Bridge durch HA steuern“ einschalten: HA erneuert alle 30 Sekunden den Spektrum-Befehl mit 90 Sekunden Gültigkeit. Ausschalten kehrt sofort zum Bridge-Tagesprofil zurück. Ein HA-Ausfall führt nach maximal 90 Sekunden plus HTTP-Laufzeit zum lokalen Programm zurück.

Ein dauerhaft manuell aktivierter Bridge-Modus lehnt HA-Leases mit HTTP 409 ab; zuerst „Tagesprofil fortsetzen“ drücken. Ein HA-Tagesprogramm kann später die Helferwerte zeitgesteuert verändern. Das Bridge-Tagesprofil ist das lokal hinterlegte Fallback-Profil, es wird nicht automatisch aus der alten HA-Integration importiert.

## Lokale API

Alle `/api/*`-Aufrufe brauchen `Authorization: Bearer TOKEN`. HTTP nur im vertrauenswürdigen Heimnetz verwenden, keine Portfreigabe. Es gibt kein MQTT und keine Cloudanmeldung.

| Methode / Pfad | Funktion |
| --- | --- |
| GET `/api/status` | Version, Modus, Restzeit, Temperatur, zuletzt gesendete Pegel, Konfiguration, Log |
| POST `/api/wifi` | `{ "ssid": "WLAN", "password": "…" }` |
| POST `/api/config` | Vollständige Konfiguration mit IP, serial, channels, timezone, points und scenes |
| POST `/api/control` | `{ "action": "preview", "seconds": 300, "levels": [10,40,0,0,70,5] }` |

Weitere Aktionen: `manual` ohne Ablauf, `lease` mit 1–90 Sekunden und `resume`. Vorschau 1–3600 Sekunden. Manuell/Vorschau/Lease sind eine einzelne Override-Ebene: ein neuer Befehl ersetzt sie. Rückkehr nach Ende immer zum Bridge-Tagesprofil. Die Laufzeit beginnt mit Annahme auf dem ESP32; HTTP-Kommunikation zur Lampe braucht zusätzliche Zeit.

## Entwickeln und Prüfen

```sh
python3 -m pip install platformio==6.1.18
cd reef-bridge
pio run
pio run --target upload --upload-port /dev/cu.usbserial-0001
pio device monitor --baud 115200
```

Die Weboberfläche wird beim Build eingebettet. GitHub Actions baut die Firmware, erzeugt ein Factory-Abbild und veröffentlicht den Web-Installer mit GitHub Pages. Versionen für die Bridge werden unabhängig von der HA-Integration gepflegt.

Vor regulärem Betrieb testen: Seriennummer-Sperre, sechs/acht Kanäle, Tageswechsel, NTP nach Neustart, Abbruch einer Vorschau, Fallback beim Stoppen von HA, WLAN-Ausfall und mindestens 24 Stunden Dauerbetrieb. Der aktuelle Code wurde noch nicht an einem angeschlossenen ESP32/Lampen-Paar abgenommen. Keine automatischen Schreibtests an einem realen Aquarium.

Lizenz: MIT, siehe [LICENSE](../LICENSE). Unabhängiges Community-Projekt ohne Herstellerzugehörigkeit.
