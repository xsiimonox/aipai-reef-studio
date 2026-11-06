# Changelog

Alle nennenswerten Änderungen dieses Projekts werden hier dokumentiert.

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
