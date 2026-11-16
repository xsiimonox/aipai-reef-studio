# Erkenntnisse aus AIPAI APK 3.1.93

Diese Notiz dokumentiert ausschließlich nachvollziehbare Protokollfakten aus der
Hersteller-App. Die APK und entschlüsselte Herstellerdateien werden nicht im
Repository verteilt.

## Native Tageskurve und Zusatz-Schalttimer

- `read=config` liefert pro Kanal 24 ganzzahlige Stundenwerte. Die App berechnet
  Zwischenwerte linear und verbindet Stunde 23 wieder mit Stunde 0.
- Die Felder nach den Kurven sind ein zusätzlicher Ein- und Ausschalt-Timer.
  Die App deaktiviert ihn mit `0` / `0`. Ein Endwert `23` bedeutet dagegen eine
  reale Abschaltung um 23 Uhr.
- AiPai Studio speichert deshalb die vollständige 24-Stunden-Kurve und setzt den
  Zusatz-Timer auf `00–00`. Licht nach 23 Uhr ist zulässig. Der gespeicherte
  Zustand wird anschließend zurückgelesen und verglichen.
- Bereits gespeicherte Profile aus Version 0.24.x bis 0.27.x können noch den
  alten Endwert 23 enthalten. Das Profil einmal erneut mit **Auf Lampe speichern
  & aktivieren** übertragen.

## Uhr und Zeitzone

- Die App synchronisiert die Uhr mit `clock=<Unix-Zeit>`.
- Das Konfigurationsformat enthält zusätzlich einen ganzzahligen UTC-Offset.
  Neuere Hersteller-Firmware kennt außerdem eine MQTT-Nachricht
  `{"type":"timeZoneSet","timeZone":"…"}` und startet danach neu.
- AiPai Studio verwendet weiterhin den schonenden, lokal bestätigten Weg:
  vorhandenen Geräte-Offset auslesen, die Unix-Zeit für die konfigurierte
  IANA/POSIX-Zeitzone kompensieren, Uhr zurücklesen und maximal zwei Minuten
  Abweichung akzeptieren. So ist kein unaufgeforderter Lampenneustart nötig und
  Sommerzeit sowie halbstündige Zeitzonen bleiben abbildbar.

## Physische Bedienelemente

- `mculed=on|off` schaltet die Beleuchtung der A8-SE-Tasten.
- `turnoffset=0|1` ist **keine Tastensperre**. `1` lässt die Lampe nach einem
  physischen Ausschalten ihr gespeichertes Profil wieder aufnehmen; `0` lässt
  sie ausgeschaltet. Die App liest beide Einstellungen nicht zurück. Studio und
  HA zeigen daher nur den zuletzt erfolgreich gesendeten Wert.

## Fabrikneue Lampe ohne Hersteller-App

Im Access-Point-Modus verwendet die App diese lokalen HTTP-Aufrufe:

1. `http://192.168.4.1/?sta=aplist` – 2,4-GHz-Netze suchen.
2. `?sta=apconnect&ssid=<URL-kodiert>&pwd=<URL-kodiert>&callback=1` – Zugangsdaten
   übertragen.
3. `?sta=getip&callback=1` – IP, Seriennummer und Hersteller-Aktivierungsstatus
   abfragen.
4. `?node=restart` – Lampe neu starten.

Reef Bridge 0.10.0 setzt diesen Ablauf als geführte lokale Einrichtung um. Ihr
eigenes Setup-Netz verwendet dabei `192.168.8.1`, während sich die Bridge als
WLAN-Client mit der Lampe auf `192.168.4.1` verbindet. Vor der Übertragung prüft
sie Adresse, Konfigurationsformat, Seriennummer und Modell; anschließend liest
sie die neue IP und dieselbe Seriennummer zurück. Das Lampen-Hotspot-Passwort
wird nicht gespeichert oder protokolliert. Weil sich Hersteller-Firmwarestände
unterscheiden können, bleibt die Funktion experimentell, bis der Ablauf an
echten Testlampen bestätigt ist.

## Gruppen und Status

- Hersteller-Gruppen sind App-seitige Listen. Befehle werden nacheinander an
  jedes kompatible Mitglied gesendet; es gibt keine belegte direkte Kopplung
  zwischen Lampen.
- Temperatur, Modell, Seriennummer, Lampenuhr, UTC-Offset, Kurven und
  Zusatz-Schaltzeiten stammen aus `read=config` und sind bereits im Studio-
  Diagnoseexport enthalten.
- Eine lokal bestätigte Firmwareversionsnummer und eine Fehlertabelle liefert
  `read=config` nicht. Entsprechende Angaben aus der Hersteller-Datenbank bleiben
  deshalb als nicht lokal bestätigt gekennzeichnet.
