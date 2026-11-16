# Lampen-Firmware: Versionsübersicht und Installationssperre

Im Studio unter **Lampen-Updates** erscheint jede Lampe einzeln, auch wenn oben eine Gruppe ausgewählt ist. **Versionen prüfen** fragt die Hersteller-Datenbank ab. Erst dieser ausdrückliche Aufruf überträgt die jeweilige Seriennummer an `ahome.doseen.com`. Die Prüfung startet kein Lampenupdate und verändert weder Lichtprofil noch Lampenuhr. Zwischen Abfragen derselben Lampe liegen mindestens 60 Sekunden. Ein Internetzugang der Lampe selbst ist für diese Metadatenabfrage nicht nötig; HA bzw. Bridge benötigen Zugang zum Herstellerserver.

## Welche Version wird angezeigt?

- **Aktuell installiert · lokal bestätigt:** Unbekannt. Die bisher analysierte Schnittstelle liefert keinen belegten lokalen Firmware-Versionsnachweis.
- **Zuletzt beim Hersteller gemeldet:** `tbl_device.d_ver`. Dieser Datenbankstand kann veraltet sein, insbesondere wenn die Lampe keinen Internetzugang hat.
- **Neueste Hersteller-Version:** `tbl_class.status`, ermittelt über die exakte Geräteklasse `d_class` des Datensatzes für die Seriennummer.
- **Update verfügbar:** Vergleich dieser beiden Herstellerangaben, kein Beweis für ein tatsächlich nötiges oder kompatibles Update.

„Geprüft“ bezeichnet den Zeitpunkt der Abfrage, nicht die letzte Kommunikation zwischen Lampe und Hersteller. Fehler, fehlende Datensätze oder nicht interpretierbare Versionen bleiben unbekannt; daraus wird niemals „kein Update“ abgeleitet. Metadaten werden nicht als HTML dargestellt und nicht als Downloadadresse oder Dateiname ausgeführt.

## Warum ist „Firmware manuell installieren“ gesperrt?

Die konkrete Prüfung am 16.11.2026 ergab:

- Die von APK 3.1.93 verwendete HTTP-Abfrage liefert Modellinformationen. Der HTTPS-Endpunkt war nicht erreichbar. Die Antwort besitzt deshalb keinen gesicherten Herkunftsnachweis.
- Die APK startet Updates mit dem modellabhängigen Dateinamen über `version=...bin`. Eine verifizierbare Signatur, ein authentifizierter Hash oder ein sicherer Downloadweg ist in diesem Ablauf nicht belegt.
- Die App deutet teils sogar eine ausbleibende Antwort als Erfolg. Das ist keine verlässliche Erfolgskontrolle und wird nicht übernommen.

Deshalb enthält diese Version **keinen freigegebenen OTA-Installationsweg**. Der Button bleibt bewusst deaktiviert; der Backend-Aufruf `firmware_install` wird ebenfalls abgelehnt, selbst wenn er direkt oder für eine Gruppe gesendet wird. Es werden keine Firmwaredateien geladen, keine MQTT-Updatebefehle veröffentlicht und keine `version=`-Anfragen an Lampen gesendet. Ein Dialog mit einer Risiko-Bestätigung ersetzt keinen fehlenden Firmware-Herkunftsnachweis.

Für eine sichere Freigabe fehlen noch ein überprüfbares Hersteller-Firmwarepaket einschließlich exakter Hardwarezuordnung und Integritäts-/Herkunftsnachweis sowie eine belegte lokale Versionsabfrage und Erfolgskontrolle. Eine Updatefunktion, die diese Nachweise ignoriert, wäre mit dem gewünschten sicheren Betrieb nicht vereinbar.

Der APK-Befehlsnachweis ist dokumentiert, ohne APK oder entschlüsselte Herstellerdateien weiterzuverteilen. Die lokale Beleuchtungssteuerung bleibt unabhängig von diesem optionalen Menü.
