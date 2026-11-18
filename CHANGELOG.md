# Changelog

Alle nennenswerten Änderungen dieses Projekts werden hier dokumentiert.

## [0.28.5] - 2026-11-18

### Fixed

- Der Konfigurationsfluss lädt wieder auf Home-Assistant-Versionen, die die optionale Zeroconf-Typklasse noch nicht bereitstellen. Dadurch verschwindet „Der Konfigurationsfluss konnte nicht geladen werden: Invalid handler specified“; die automatische Reef-Bridge-Erkennung bleibt erhalten.

## [0.28.4] - 2026-11-18

### Fixed

- Repariert den Import der Reef-Bridge-Ergänzung auf aktuellen Home-Assistant-Versionen. Dadurch laden vorhandene AIPAI-Lampeneinträge wieder gemeinsam statt mit „Einrichtungsfehler: Import error“ fehlzuschlagen.

## [0.28.3] - 2026-11-17

### Added

- Reef Bridge wird im lokalen Netz über mDNS automatisch erkannt und kann als eigenes Home-Assistant-Gerät hinzugefügt werden. Die Geräteseite enthält Status, Firmware-/Lampenanzahl und einen direkten Link zur Bridge-Weboberfläche.

## [0.28.2] - 2026-11-17

### Fixed

- Das Verschieben eines Kanal- oder Gesamtintensitätsreglers sendet die Live-Vorschau weiter, sperrt aber nicht mehr die gesamte Oberfläche mit der Übertragungsanimation. Während ein Vorschau-Befehl läuft, wird nur der zuletzt eingestellte Wert nachgereicht.

## [0.28.1] - 2026-11-16

### Added

- AiPai Studio zeigt bei jeder einzelnen Lampe die aktuelle lokale IP-Adresse sowohl in der Lampenauswahl als auch im Erreichbarkeitsstatus an. Gruppen zeigen bewusst keine einzelne IP.
- Das Modell A7 S3 Mini (`A7S3`) wird als Sechskanal-Lampe ohne Mondlichtkanal erkannt.

### Fixed

- Späte Geräte-Rückmeldungen nach dem Neuladen der Integration überspringen bereits entladene Home-Assistant-Entities, damit die übrigen Lampen weiter aktualisiert werden.

## [0.28.0] - 2026-11-16

### Fixed

- Native Tagesprogramme setzen den separaten Ein-/Ausschalt-Timer jetzt auf `00–00` (aus). APK 3.1.93 bestätigt, dass der bisherige Endwert 23 eine echte Abschaltung um 23 Uhr auslöst. Profile dürfen nun über 23 Uhr und Mitternacht laufen; Kurve und deaktivierter Timer werden zurückgelesen und geprüft.
- Der Diagnosebereich warnt vor noch vorhandenen Zusatz-Schaltzeiten und fordert zum erneuten Speichern des Tagesprogramms auf.

### Added

- Steuerung „Profil nach physischem Ausschalten fortsetzen“ als Studio-Gruppenaktion und eigene Home-Assistant-Entität. Der belegte Befehl `turnoffset=0|1` wird korrekt als Verhalten nach Taste/Drehknopf bezeichnet, nicht als Tastensperre; Anzeige nur als zuletzt gesendeter Wert.
- Dokumentierte APK-Erkenntnisse zu Uhrzeit, Zeitzone, nativen Kurven, fabrikneuer WLAN-Einrichtung, Statusfeldern und Hersteller-Gruppen.

## [0.27.1] - 2026-11-16

### Changed

- Ein gemeinsamer Button „Updates prüfen“ ersetzt die Einzelprüfungen und prüft alle Lampen unabhängig von der Auswahl. Fehler oder eine noch aktive Abfragesperre bei einer Lampe verhindern nicht die Prüfung der übrigen.
- Kompakte Lampenliste mit Name, Versionsvergleich und Status. Modell, Seriennummer, Zeitstempel, Hinweise und Installationssperre stehen in aufklappbaren Details; geöffnete Zeilen bleiben beim Aktualisieren erhalten.

## [0.27.0] - 2026-11-16

### Added

- Menü „Lampen-Updates“: jede Lampe separat, Hersteller-Modell, in der Hersteller-Datenbank gemeldete Firmware, neueste Modellversion, Hinweise und Zeitpunkt der manuellen Prüfung.
- Herstellerabfragen sind explizit manuell, auf genau die Seriennummer beschränkt, größenbegrenzt und für 60 Sekunden gegen Wiederholung gesperrt. Unbekannte Versionen und Abfragefehler werden nicht als „aktuell“ ausgegeben.
- Manueller Installationsbutton mit sichtbarer Sperrbegründung. Firmwareinstallation bleibt auch im Backend gesperrt: Der Hersteller bietet die geprüfte Versionsabfrage nur per HTTP, ohne belegte signierte Firmware und ohne lokalen Versionsnachweis. Dies ist eine Versionsübersicht, kein freigegebener OTA-Flasher.

## [0.26.0] - 2026-11-16

### Added

- Lokaler Home-Assistant-Schalter „Tastenbeleuchtung“ und explizite Ein-/Aus-Aktionen im Studio, auch für Gruppen. Der dedizierte PilotLamp-Befehl verändert keine Spektralkanäle, Tageskurven oder Betriebsmodi.
- Herstellerbefehl aus AIPAI APK 3.1.93 verifiziert: `mculed=on` / `mculed=off`. Anzeige ist die zuletzt erfolgreich gesendete Einstellung, kein ausgelesener Istzustand. Nach Neustart unbekannt; kein automatisches Wiedersenden. Wirkung auf Hardware noch durch Nutzer zu bestätigen.

## [0.25.0] - 2026-11-15

### Added

- Ladeanimation mit gesperrter Oberfläche während schreibender Übertragungen, einschließlich Live-Vorschau, Gruppen und Bridge-Einrichtung. Tastatur- und Mausbedienung werden bis zur Antwort blockiert; nach Erfolg oder Fehler wird die Oberfläche freigegeben.
- Doppelte Befehle werden abgefangen, ausstehende Slider-Vorschauen beim Start einer anderen Aktion verworfen. Die Sperre bleibt auch beim Neuzeichnen der Oberfläche erhalten.

## [0.24.1] - 2026-11-11

### Fixed

- Statusmeldungen prüfen die tatsächliche Lampenuhr gegen die konfigurierte lokale Zeit. Abweichungen über zwei Minuten werden mit mindestens 60 Sekunden Abstand erneut korrigiert, auch wenn die Lampe durchgehend erreichbar bleibt.
- Das Protokoll unterscheidet angeforderte Uhrkorrekturen von bestätigten zurückgelesenen Uhrzeiten. Mitternacht wird beim Vergleich korrekt behandelt.

## [0.24.0] - 2026-11-11

### Fixed

- Tagesprogramme setzen alte native Schaltzeiten auf den unterstützten Bereich 00–23 Uhr zurück und synchronisieren die Uhr beim Speichern. Profile mit Licht nach 23 Uhr werden im nativen Modus ausdrücklich abgelehnt; hierfür externe Zeitplansteuerung verwenden, bis der Firmware-Grenzfall geprüft ist.
- Zurückgelesene Kurven, Kanalwerte, Schaltzeiten und Zeitzone werden nach dem Speichern verglichen. Ein abweichender Speicherinhalt wird nicht mehr als erfolgreiche Aktivierung angezeigt.

### Added

- Diagnose-Download direkt im Studio, auch ohne Zwischenablage und für alle Lampen gleichzeitig; enthält Bibliothek, Lampenuhr, Zeitzonen, Schaltzeiten und Ereignisprotokoll.
- Native HA-Diagnose enthält Studio-Zustand und Ereignisse. Protokoll erfasst Kanal-/Kurvenänderungen und Steuerbefehle; Verlauf gilt für die laufende Sitzung.

## [0.23.0] - 2026-11-08

### Changed

- Die sichtbare Produktbezeichnung lautet durchgängig **AiPai Studio** – in der Home-Assistant-Integration, Seitenleiste, Update-Entität, HACS, Bridge-Weboberfläche, Dokumentation und den aktualisierten Screenshots.
- Bestehende technische IDs, Installationspfade und Repository-Adressen bleiben stabil, damit vorhandene Installationen und automatische Updates ohne Migration weiter funktionieren.

## [0.22.0] - 2026-11-08

### Added

- „Auf Lampe speichern & aktivieren“ ist der neue Standard für Tagesprogramme. AiPai Studio überträgt eine 24-Punkte-Stundenkurve in den persistenten Speicher und schaltet den lampeneigenen Automatikmodus ein. Das Programm läuft ohne HA, Cloud, MQTT oder Internet weiter.
- Die Oberfläche zeigt ausdrücklich „Auf Lampe gespeichert“ beziehungsweise den sicheren Startmodus an.

### Fixed

- Nach einem Home-Assistant-Neustart werden alte HA-gesteuerte Programme, Szenen oder Vorschauen nicht mehr automatisch erneut geschrieben. Der tatsächliche Lampenzustand bleibt unverändert, bis der Nutzer eine Aktion ausführt.
- Änderungen an einem bereits auf der Lampe aktiven Profil werden beim Speichern erneut übertragen. Gemischte Gruppen erhalten pro Lampe nur ihre unterstützten Kanäle.

## [0.21.3] - 2026-11-07

### Added

- Zeitpunkte eines Tagesprogramms lassen sich duplizieren. Die Kopie übernimmt sämtliche Kanalverhältnisse und wird auf die nächste freie Minute gelegt, damit anschließend nur die Zielzeit angepasst werden muss.

### Changed

- Die Gesamtintensität skaliert jetzt proportional alle Kanäle an allen Zeitpunkten des vollständigen Tagesprogramms. Der eingestellte Faktor bleibt beim Wechsel zwischen Stützpunkten erhalten. Gilt für HA und Reef Bridge 0.2.2.

## [0.21.2] - 2026-11-07

### Fixed

- Gemischte Gruppen aus 6- und 8-Kanal-Lampen zeigen alle acht Kanäle im Programm- und Szeneneditor. UV und Warm White werden ausschließlich an unterstützende Lampen gesendet; Sechskanal-Lampen ignorieren die zusätzlichen Werte. Gilt für HA und Reef Bridge 0.2.1.
- Der gemeinsame Vorschau-Timer bleibt auch bei unterschiedlich langen Kanalvektoren in gemischten HA-Gruppen sichtbar und abbrechbar.

## [0.21.1] - 2026-11-07

### Changed

- Reef Bridge 0.2.0 verwendet jetzt denselben AiPai-Studio-Editor wie Home Assistant. Design, Programm-/Szeneneditor, Rampen-Assistent, Gesamtintensität, Gruppenbedienung und Diagnose werden aus einer gemeinsamen Quelldatei gebaut.
- CI prüft die gemeinsame Studio-Version; Änderungen am gemeinsamen Panel lösen auch den Firmware-Build der Bridge aus.

### Fixed

- Countdown läuft auch zwischen zwei Statusabfragen sekündlich weiter.

## [0.21.0] - 2026-11-07

### Changed

- AiPai Studio erhält eine ruhigere, Apple-inspirierte Oberfläche mit einheitlichen Feldhöhen, symmetrischen Editor-Spalten, segmentierter Navigation und besser lesbarer Typografie.
- Beschriftungen und Hilfetexte erklären Bibliothek, Aktivierung, Rampen und Live-Vorschau direkt am jeweiligen Bedienelement. Die Gesamtintensität und lange Lampennamen erhalten mehr Platz.
- Auf Smartphones stehen Felder und Aktionen übersichtlich untereinander; Gruppenmitglieder sind als große Auswahlflächen bedienbar.

## [0.20.6] - 2026-11-07

### Fixed

- Der Programm- und Szeneneditor ist jetzt auch bei ausgewählter Lampengruppe vollständig nutzbar: neue Profile, Bearbeiten, Kopieren, Löschen, manueller Modus und die zeitlich begrenzte Live-Vorschau funktionieren für die ganze Gruppe.

## [0.20.5] - 2026-11-07

### Added

- Home Assistant erhält für jede AIPAI-Lampe eine native Update-Entität „AiPai Studio update“. Sie prüft die neuesten veröffentlichten GitHub-Releases alle 30 Minuten und meldet verfügbare HACS-Updates unter **Einstellungen → System → Updates**, unabhängig davon, wann HACS seine Repository-Informationen aktualisiert.

### Changed

- Die Versionsanzeige in AiPai Studio prüft ebenfalls den neuesten veröffentlichten GitHub-Release statt des veränderlichen `main`-Branches und aktualisiert ihren Cache alle 30 Minuten.

## [0.20.4] - 2026-11-07

### Changed

- Programme und Szenen verwenden jetzt eine gemeinsame, Home-Assistant-weite AiPai-Studio-Bibliothek. Jedes Profil ist dadurch an jeder Lampe verfügbar und kann auch von Lampengruppen gestartet werden.
- Bestehende pro-Lampe-Bibliotheken werden beim ersten Laden automatisch und ohne Verlust in die gemeinsame Bibliothek übernommen.
- Ein 8-Kanal-MAX-Profil lässt sich auf einer 6-Kanal-Lampe verwenden (UV/Warmweiß werden dort ignoriert); fehlende MAX-Kanäle eines 6-Kanal-Profils bleiben auf einer 8-Kanal-Lampe bei 0 %.

## [0.20.3] - 2026-11-07

### Fixed

- Lampengruppen dürfen jetzt auch genau eine Lampe enthalten. Leere Gruppen und doppelt ausgewählte Lampen werden weiterhin verhindert.

## [0.20.2] - 2026-11-07

### Fixed

- AiPai Studio übernimmt aktuelle Gerätenamen aus der Home-Assistant-Geräteregistrierung. Umbenannte Lampen erscheinen beim nächsten Panel-Refresh auch in der Lampen- und Gruppenauswahl mit dem neuen Namen; Gruppenmitgliedschaften bleiben über die Seriennummer erhalten.

## [0.20.1] - 2026-11-07

### Changed

- Die Auswahl zwischen linearer und sinusförmiger Rampe wurde entfernt. Tagesprogramme verwenden ausschließlich lineare Übergänge.

## [0.20.0] - 2026-11-07

### Changed

- Die Übergangsform ist jetzt eine globale Eigenschaft des gesamten Tagesprogramms statt einer Einstellung pro Stützpunkt. „Sinus, weich“ glättet damit den vollständigen Verlauf einheitlich.
- Die Kurvengrafik wird im Zwei-Minuten-Raster berechnet und zeichnet weiche Übergänge sichtbar glatt statt stufig.

## [0.19.1] - 2026-11-07

### Added

- AiPai Studio zeigt die von der Lampe gemeldete interne Gerätetemperatur direkt neben dem Erreichbarkeitsstatus an.

## [0.19.0] - 2026-11-07

### Added

- Diagnose-Tab in AiPai Studio mit kopierbarem Ereignisprotokoll. Er zeichnet Lampenstatus- und Moduswechsel, insbesondere `power=off`, lokale Verbindungsabbrüche, Wiederverbindungen, AiPai-Studio-Aktionen und Ausgabefehler auf.

## [0.18.7] - 2026-11-07

### Fixed

- Laufende Tageszeit-Vorschauen haben einen eigenen Abbruchknopf. Er beendet ausschließlich den Timer und setzt sofort das darunter aktive Tagesprogramm fort.

## [0.18.6] - 2026-11-07

### Fixed

- Die Tagesprogramm-Grafik zeichnet sinusförmige Rampen nun tatsächlich als abgeflachte Kurven statt sie optisch als gerade Linien zwischen Stützpunkten darzustellen.

## [0.18.5] - 2026-11-07

### Added

- Tagesverlauf-Vorschau auf der Lampe: Eine frei gewählte Uhrzeit aus der Kurve lässt sich für 1 bis 60 Minuten direkt auf der Lampe anzeigen. Der Restzeit-Countdown zeigt die exakte Rückkehr zum Tagesprofil.

## [0.18.4] - 2026-11-07

### Fixed

- AiPai Studio sendet bei jedem Minuten-Takt einen Keep-alive für aktive Tagesprogramme, Szenen, Live-Vorschauen und den dauerhaften manuellen Modus. Dadurch verwirft die Lampen-Firmware statische lokale Pegel nicht mehr nach einigen Minuten.

## [0.18.3] - 2026-11-07

### Added

- Tagesprogramme haben pro Stützpunkt einen Gesamtintensitäts-Regler. Er skaliert alle Kanäle proportional und erhält damit das eingestellte Spektralverhältnis.

## [0.18.2] - 2026-11-07

### Fixed

- Die Statuskarte „Aktiver Lichtmodus“ aktualisiert sich bei jedem Moduswechsel sofort, ohne dass AiPai Studio neu geöffnet werden muss.

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

- AiPai Studio zeigt die tatsächlich geladene Oberflächen-Version dauerhaft im Kopfbereich an.

## [0.17.3] - 2026-11-07

### Added

- AiPai Studio zeigt einen Hinweis an, sobald eine neuere veröffentlichte GitHub-Version verfügbar ist. Die Abfrage erfolgt maximal alle sechs Stunden im Hintergrund und beeinflusst die lokale Lampensteuerung nicht.

## [0.17.2] - 2026-11-07

### Added

- Szenenbibliothek mit einer sichtbaren Schnellwechsel-Leiste: Jede gespeicherte Szene kann mit einem Klick für 15 Minuten gestartet werden. Die aktive Szene ist hervorgehoben; über die Auswahl bleibt sie unabhängig bearbeitbar.

### Fixed

- Die native AiPai-Studio-Panel-Datei erhält eine neue Cache-Version, damit Home Assistant nach dem Update zuverlässig die aktuelle Szenenbibliothek lädt.

## [0.17.1] - 2026-11-07

### Fixed

- Szenen werden nun serverseitig sekundengenau beendet, statt erst beim nächsten Minuten-Takt geprüft zu werden. Beim Start liest AiPai Studio die gewählte Dauer direkt aus dem Eingabefeld.

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

- Bereits aktive Tagesprogramme und Szenen können bearbeitet und gespeichert werden. AiPai Studio berechnet die neuen Werte danach sofort neu und übernimmt sie auf die Lampe; nur das Löschen eines aktiven Eintrags bleibt geschützt.

## [0.16.2] - 2026-11-07

### Fixed

- Neue Tagesprogramme und Szenen lassen sich wieder in der Bibliothek speichern; die interne Bibliotheks-ID kollidiert nicht mehr mit der DOM-Eigenschaft eines HTML-Elements.

## [0.16.1] - 2026-11-07

### Fixed

- AiPai Studio wählt nach dem Laden der WebSocket-Antwort wieder zuverlässig die erste eingerichtete Lampe aus, statt fälschlich den Leerzustand anzuzeigen.

## [0.16.0] - 2026-11-06

### Added

- Home-Assistant-Klassifizierung für Beleuchtungssysteme und Aquarien.
- AiPai Studio als native Home-Assistant-Oberfläche für Programme, Szenen und die Live-Tagesansicht.
- Lineare und sinusförmige Rampen für jeden Übergang eines Tagesprogramms.
- Lampengruppen mit synchronisiertem Start gleich benannter Programme und Szenen.
- Einstellbare IANA-Zeitzone pro Lampe.
- Serielle lokale HTTP-Ausführung mit Identitätsprüfung vor persistenten Schreibvorgängen.

### Changed

- Die problematischen Legacy-Schalter und -Buttons sind nicht mehr Teil der regulären Geräteansicht.
- Die lokale Steuerung bleibt bei fehlender Netzwerkerkennung lokal und wartet auf eine erneute Erkennung, statt in die Hersteller-Cloud zurückzufallen.
