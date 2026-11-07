# Changelog

Alle nennenswerten Änderungen dieses Projekts werden hier dokumentiert.

## [0.18.5] - 2026-11-07

### Added

- Tagesverlauf-Vorschau auf der Lampe: Eine frei gewählte Uhrzeit aus der Kurve lässt sich für 1 bis 60 Minuten direkt auf der Lampe anzeigen. Der Restzeit-Countdown zeigt die exakte Rückkehr zum Tagesprofil.

## [0.18.4] - 2026-11-07

### Fixed

- Reef Studio sendet bei jedem Minuten-Takt einen Keep-alive für aktive Tagesprogramme, Szenen, Live-Vorschauen und den dauerhaften manuellen Modus. Dadurch verwirft die Lampen-Firmware statische lokale Pegel nicht mehr nach einigen Minuten.

## [0.18.3] - 2026-11-07

### Added

- Tagesprogramme haben pro Stützpunkt einen Gesamtintensitäts-Regler. Er skaliert alle Kanäle proportional und erhält damit das eingestellte Spektralverhältnis.

## [0.18.2] - 2026-11-07

### Fixed

- Die Statuskarte „Aktiver Lichtmodus“ aktualisiert sich bei jedem Moduswechsel sofort, ohne dass Reef Studio neu geöffnet werden muss.

## [0.18.1] - 2026-11-07

### Added

- Prominente Statuskarte „Aktiver Lichtmodus“: Sie zeigt jederzeit Live-Vorschau, aktive Szene, Tagesprogramm, dauerhaften manuellen Modus, Wiederherstellung oder den lampeneigenen Modus an.

## [0.18.0] - 2026-11-07

### Added

- „Manueller Modus aktivieren“ hält eine gespeicherte Szene dauerhaft, bis sie über „Szene abbrechen – Tagesprogramm fortsetzen“ beendet wird. Zeitlich begrenzte Szenen bleiben weiterhin über die Laufzeit-Auswahl verfügbar.

### Changed

- Die Live-Vorschau beim Bearbeiten eines Tagesprogramms endet nach einer Minute und kehrt dann zum Tagesprofil zurück. Beim Bearbeiten einer Szene bleibt die Vorschau fünf Minuten aktiv.

## [0.17.4] - 2026-11-07

### Added

- Reef Studio zeigt die tatsächlich geladene Oberflächen-Version dauerhaft im Kopfbereich an.

## [0.17.3] - 2026-11-07

### Added

- Reef Studio zeigt einen Hinweis an, sobald eine neuere veröffentlichte GitHub-Version verfügbar ist. Die Abfrage erfolgt maximal alle sechs Stunden im Hintergrund und beeinflusst die lokale Lampensteuerung nicht.

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
