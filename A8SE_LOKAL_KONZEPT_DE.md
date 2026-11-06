# AIPAI A8SE / A8SE II Max / A7: lokale Home-Assistant-Steuerung

## Reef Studio

Kopiere den vollständigen Ordner `custom_components/aipai_light` in das
Konfigurationsverzeichnis von Home Assistant und starte Home Assistant neu.
Aktiviere bei der Lampe **Local control**, wähle das passende Kanalprofil und
öffne **Reef Studio** in der Seitenleiste mit einem Administratorkonto.

- **Tagesprogramme:** Lege Stützpunkte mit Uhrzeit und Prozentwert pro Kanal an.
  Der Rampen-Assistent legt anhand des ausgewählten Spektrums je zwei Punkte
  für Auf- und Abdimmen an. Die Rampen können linear oder als weiche
  Sinuskurve laufen. Speichern legt den Entwurf in der Bibliothek ab.
  Erst **Programm aktivieren** übernimmt die Ausgabe.
- **Szenen:** Speichere ein Spektrum, etwa für Fotos oder Blaulicht, und starte
  es für 1 bis 180 Minuten. Anschließend wird das Tagesprogramm fortgesetzt
  oder der vorherige Lampenmodus wiederhergestellt.
- **Anzeige:** Der türkise Marker zeigt die Uhrzeit in der HA-Zeitzone. Der
  Vorschau-Regler zeigt Sollwerte für jede Minute. Die Kanalpegel daneben
  unterscheiden HTTP-bestätigte Vorgaben und ausgelesene manuelle Werte.
  Die Firmware liefert keine verlässliche Messung der momentanen LED-Leistung.
- **Gruppen:** Unter *Gruppen* mehrere Lampen zusammenfassen. Ein
  Gruppenprogramm oder eine Gruppenszene startet jeweils den gleich benannten
  Eintrag auf allen Mitgliedern. Jede Lampe behält ihr eigenes Kanalprofil.
- **Zeitzone:** In den Optionen einer Lampe die IANA-Zeitzone setzen, etwa
  `Europe/Berlin`. Tagesprogramme und der Zeitabgleich berücksichtigen damit
  Sommer- und Winterzeit dieser Lampe.

Home Assistant berechnet aktive Studio-Programme minutengenau. Bei einem
HA- oder Netzwerkausfall können keine neuen Werte übertragen werden. Nach der
Wiederverbindung versucht das Studio, den aktuellen Sollwert zu setzen.
**Lampeneigenen Modus wiederherstellen** gibt die Steuerung zurück. Die
lampeneigene Tageskurve bleibt bei der Umschaltung erhalten.

## Ergebnis

Die Lampen lassen sich ohne Flashen des ESP und ohne Abhängigkeit von der
Hersteller-Cloud steuern. Die untersuchte Firmware stellt im lokalen WLAN einen
HTTP-Endpunkt bereit. Home Assistant liest den Zustand mit
`GET http://<lampen-ip>/?read=config` und setzt Kanäle direkt mit
`GET http://<lampen-ip>/?<kanal>=<wert>` (Wert `0..1023`).

Die bereits enthaltene Integration verwendet genau diesen lokalen Weg, findet
Lampen im Netz, erkennt das Modell und legt pro Spektralkanal einen Regler an.
Sie verwaltet außerdem Power, manuell/geplant, Zeitabgleich und Lichtkurven.
Kein MQTT-Broker, kein Cloud-Konto und kein Firmware-Flash sind dafür nötig.

### Kanalprofil wählen

Die Controllerkennung ist nicht bei allen Geräten eindeutig: Die Testlampe
meldet `A8SE8`, obwohl sie physisch sechs Kanäle hat. Unter den Optionen der
Integration deshalb **6 channels** wählen. Das Profil lautet:
White, Blue, Red, Green, Royal Blue, Purple. Für A8SE II MAX/MAX Pro ist
**8 channels** vorgesehen, ergänzt um UV und Warm White. Das Profil begrenzt
die in Home Assistant sichtbaren und von Szenen angesprochenen Kanäle, während
die Firmware intern weiter ihren vollständigen Status liefern darf.

## Architektur

```text
Home Assistant ── HTTP im lokalen WLAN ──> AIPAI-Lampe (ESP)
       │                                          │
       └── optionale Firewall/DNS-Regel ──X── Hersteller-Cloud/MQTT
```

MQTT ist deshalb nicht der bevorzugte Einstieg: Das Hersteller-MQTT ist ein
gemeinsam genutzter Cloud-Broker. Die lokale HTTP-Schnittstelle vermeidet
Credentials, Ausfälle der Cloud und Kollisionen mit der Hersteller-App.

## Rollout in sicherer Reihenfolge

1. **Netz vorbereiten:** Lampe und Home Assistant ins gleiche LAN/VLAN bringen;
   der Lampe im Router eine DHCP-Reservierung geben. Zunächst die Hersteller-
   Cloud noch nicht sperren.
2. **Nur lesen:** Auf einem Rechner im gleichen Netz ausführen:

   ```bash
   python3 tools/aipai_probe.py <IP-DER-LAMPE>
   ```

   Das Tool ist absichtlich rein lesend. Es bestätigt Modell, Seriennummer,
   Kanalzahl und aktuelle Pegel.
3. **Integration installieren:** Den Ordner
   `custom_components/aipai_light` nach
   `/config/custom_components/aipai_light` der Home-Assistant-Installation
   kopieren, Home Assistant neu starten und unter *Einstellungen → Geräte &
   Dienste → Integration hinzufügen* nach **AIPAI Aquarium Light** suchen.
4. **Lokal suchen:** In der Integration *Search network* wählen. Sie fragt nur
   `/?read=config` ab. Die gefundene Lampe auswählen und anschließend in den
   Optionen **Local control** einschalten.
5. **Kontrolliert testen:** Zuerst einen einzelnen Kanal um 1–2 % ändern,
   Rückmeldung und tatsächliche LED prüfen, dann die Regler benennen und die
   Tageskurve anlegen. Vor einem Speichern der Kurve einen Screenshot bzw.
   Export des vorhandenen Hersteller-Profils sichern.
6. **Cloud isolieren:** Erst wenn mehrere Schaltvorgänge sowie ein Neustart von
   Home Assistant funktionieren, ausgehend nur die Lampen zur Hersteller-Cloud
   per Firewall oder lokaler DNS-Sperre blockieren. Lokales HTTP zwischen HA
   und den Lampen bleibt erlaubt. Kein Internet-Ausfall für andere Geräte nötig.

## Sicherheits- und Betriebsregeln

- Die Lampen nicht aus dem Internet erreichbar machen und keine Portweiterleitung
  auf Port 80 einrichten.
- Für jede Lampe eine feste IP/DHCP-Reservierung setzen; die Integration kann
  zwar erneut suchen, eine feste Zuordnung ist aber robuster.
- Bei A7S III müssen die beiden physischen Drehregler auf Minimum stehen,
  ansonsten überstimmen sie die WLAN-Steuerung.
- Die eingebauten Relais/LED-Treiber werden nicht angefasst: keine Garantie-
  oder Flash-Risiken durch Hardware-Modifikation.
- Die Cloud-Sperre ist reversibel: Firewall-/DNS-Regel entfernen, falls die
  Hersteller-App erneut benötigt wird.

## Was wir am echten Gerät noch validieren müssen

Die lokale API und A8SE8/A7S III sind bereits praktisch bestätigt. Für A8SE,
A8SE II Max und die konkrete A7-Variante validieren wir mit dem Read-only-Probe
die gemeldete Modellkennung und die Kanalbelegung. Danach folgen nur kleine
Live-Änderungen je Kanal. So bekommen wir die korrekten Labels (UV/Violett/
Royal Blue/Blau/Cyan/Weiß/Rot/Grün je Modell), ohne Korallen durch falsche
Kanäle oder Sprünge zu belasten.

## MQTT-Fallback (nur falls die HTTP-API auf einer Firmware fehlt)

Dann wird der Datenverkehr der offiziellen App mit einem isolierten Testgerät
protokolliert und Topic/Payload werden reproduziert. Erst wenn klar ist, dass
die Lampe einen frei konfigurierbaren lokalen Broker akzeptiert, wird ein
Mosquitto-Broker in Home Assistant eingesetzt. Wir würden keine Cloud-
Credentials hartkodieren und keine fremden Geräte auf dem Hersteller-Broker
ansprechen. Dieser Weg ist daher Plan B, nicht der erste Eingriff.
