# Reef Bridge Changelog

## [0.5.1] - 2026-11-11

- Im autonomen Betrieb werden zurückgelesene Lampenuhren auf Abweichungen über zwei Minuten geprüft und mit mindestens 60 Sekunden Abstand korrigiert. Uhrkorrekturen und Fehler werden protokolliert.

## [0.5.0] - 2026-11-11

- Gemeinsamer Studio-Diagnose-Download für alle Lampen, mit Zeitstempeln und bis zu 32 begrenzten Ereignissen pro Lampe (nur aktuelle Sitzung).
- Rücklesen und Vergleich gespeicherter Tageskurven und Schaltzeiten; alte native Schaltzeiten werden auf 00–23 Uhr gesetzt. Licht nach 23 Uhr benötigt bis zur Firmware-Verifikation externe Zeitplansteuerung.

## [0.4.0] - 2026-11-08

- Die gemeinsam mit Home Assistant verwendete Oberfläche heißt jetzt durchgängig **AiPai Studio**.
- Browser-Titel, Dokumentation, Versionsanzeige und Screenshots wurden gemeinsam aktualisiert; technische URLs bleiben kompatibel.

## [0.3.0] - 2026-11-08

- Tagesprogramme werden standardmäßig als 24 Stundenpunkte dauerhaft in jeder ausgewählten Lampe gespeichert und dort im Automatikmodus ausgeführt.
- Bridge-Neustarts sind schreibgeschützt: ältere Bridge-Programme werden nicht ungefragt wieder in den manuellen Modus übernommen.
- Lampenuhr wird beim Speichern, nach Wiederverbindung und alle sechs Stunden synchronisiert. Temporäre Szenen und Vorschauen kehren zum gespeicherten Lampenprogramm zurück.
- Gemeinsamer Editor mit HA 0.22.0.

## [0.2.2] - 2026-11-07

- Gesamtintensität skaliert das komplette Tagesprogramm statt nur des ausgewählten Zeitpunkts.
- Zeitpunkte können mitsamt allen Kanalwerten dupliziert und anschließend auf eine neue Uhrzeit gelegt werden. Gemeinsamer Editor mit HA 0.21.3.

## [0.2.1] - 2026-11-07

- Gemischte Gruppen bieten alle acht Kanäle. UV und Warm White wirken nur auf MAX-Lampen; Sechskanal-Lampen erhalten ausschließlich ihre sechs Werte. Gemeinsamer Editor mit HA 0.21.2.

## [0.2.0] - 2026-11-07

- Gemeinsamer AiPai-Studio-Editor mit Home Assistant; keine zweite Designkopie.
- Programme und Szenen mit Kopieren, Bearbeiten, Gesamtintensität, Rampen-Assistent und Tageskurve.
- Bis zu vier Lampen, gemeinsame Bibliothek (bis 32 Programme / 32 Szenen innerhalb des Speicherlimits), bis 16 Gruppen, einzelne Zeitzonen und Kanalprofile.
- Vorschau 1–60 Minuten mit Countdown/Abbruch, Szenen, dauerhafter manueller Modus und HA-Lease mit Rückkehr zum lokalen Tagesprofil.
- Lokaler Dateispeicher mit temporärer Datei vor Übernahme, vollständiges Backup/Import. Bisherige 0.1-Konfiguration wird beim ersten Start übernommen.
- Firmware und Studio-Version werden getrennt angezeigt und in CI geprüft.

## [0.1.2] - 2026-11-07

- Tokenfreier Zugriff und Captive Portal im Setup-WLAN.

## [0.1.1] - 2026-11-07

- Automatische Firmware-Update-Suche.
