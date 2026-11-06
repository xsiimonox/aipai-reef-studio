# Changelog

Alle nennenswerten Änderungen dieses Projekts werden hier dokumentiert.

## [0.17.2] - 2026-11-07

### Added

- Szenenbibliothek mit einer sichtbaren Schnellwechsel-Leiste: Jede gespeicherte Szene kann mit einem Klick für 15 Minuten gestartet werden. Die aktive Szene ist hervorgehoben; über die Auswahl bleibt sie unabhängig bearbeitbar.

### Fixed

- Die native Reef-Studio-Panel-Datei erhält eine neue Cache-Version, damit Home Assistant nach dem Update zuverlässig die aktuelle Szenenbibliothek lädt.

## [0.17.1] - 2026-11-07

### Fixed

- Szenen werden nun serverseitig sekundengenau beendet, statt erst beim nächsten Minuten-Takt geprüft zu werden. Beim Start liest Reef Studio die gewählte Dauer direkt aus dem Eingabefeld.

### Added

- Aktive Szenen zeigen einen sekündlich aktualisierten Restzeit-Countdown. „Szene abbrechen – Tagesprogramm fortsetzen“ beendet sie jederzeit und kehrt sofort zum Tagesprogramm beziehungsweise vorherigen Zustand zurück.

## [0.17.0] - 2026-11-07

### Added

- Live-Vorschau beim Bearbeiten: Regleränderungen aus einem Tagesprogramm oder einer Szene werden direkt an die Lampe gesendet. Jede Änderung hält die Vorschau bis zu fünf Minuten aktiv; anschließend übernimmt wieder das vorher aktive Programm oder der ursprüngliche Lampenzustand.
- Gespeicherte Szenen können über „Manueller Modus · 60 Min.“ für genau eine Stunde aktiviert werden. Danach kehrt die Lampe automatisch zum normalen Programm beziehungsweise Zustand zurück.

### Changed

- Die frei wählbare Laufzeit einer Szene ist auf maximal 60 Minuten begrenzt.

## [0.16.4] - 2026-11-07

### Added

- Bereits gespeicherte Tagesprogramme und Szenen haben jetzt die eigene Aktion „Als neues … speichern“. Sie legt unabhängig vom umbenannten Original einen zusätzlichen Bibliothekseintrag an.

## [0.16.3] - 2026-11-07

### Fixed

- Bereits aktive Tagesprogramme und Szenen können bearbeitet und gespeichert werden. Reef Studio berechnet die neuen Werte danach sofort neu und übernimmt sie auf die Lampe; nur das Löschen eines aktiven Eintrags bleibt geschützt.

## [0.16.2] - 2026-11-07

### Fixed

- Neue Tagesprogramme und Szenen lassen sich wieder in der Bibliothek speichern; die interne Bibliotheks-ID kollidiert nicht mehr mit der DOM-Eigenschaft eines HTML-Elements.

## [0.16.1] - 2026-11-07

### Fixed

- Reef Studio wählt nach dem Laden der WebSocket-Antwort wieder zuverlässig die erste eingerichtete Lampe aus, statt fälschlich den Leerzustand anzuzeigen.

## [0.16.0] - 2026-11-06

### Added

- Home-Assistant-Klassifizierung für Beleuchtungssysteme und Aquarien.
- Reef Studio als native Home-Assistant-Oberfläche für Programme, Szenen und die Live-Tagesansicht.
- Lineare und sinusförmige Rampen für jeden Übergang eines Tagesprogramms.
- Lampengruppen mit synchronisiertem Start gleich benannter Programme und Szenen.
- Einstellbare IANA-Zeitzone pro Lampe.
- Serielle lokale HTTP-Ausführung mit Identitätsprüfung vor persistenten Schreibvorgängen.

### Changed

- Die problematischen Legacy-Schalter und -Buttons sind nicht mehr Teil der regulären Geräteansicht.
- Die lokale Steuerung bleibt bei fehlender Netzwerkerkennung lokal und wartet auf eine erneute Erkennung, statt in die Hersteller-Cloud zurückzufallen.
