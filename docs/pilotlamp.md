# Tastenbeleuchtung / PilotLamp

## Bedienung

- Home Assistant: **Einstellungen → Geräte & Dienste → AiPai Studio → Lampe**: Schalter **Tastenbeleuchtung**. Er wird für lokal gesteuerte Lampen angelegt und kann in Dashboards sowie Automationen verwendet werden.
- AiPai Studio / Reef Bridge: unter **Kanalpegel** die Aktionen **Tastenbeleuchtung ein** und **Tastenbeleuchtung aus**. Bei ausgewählter Gruppe werden die Befehle an jedes Mitglied gesendet. Eine Gruppe ist keine atomare Transaktion: Schlägt eine Lampe fehl, können vorherige Mitglieder bereits geschaltet sein.

Gedacht für die beleuchteten Tasten der A8 SE. Andere Controller können den Befehl ignorieren; die Modellkennung unterscheidet diese Hardwarefunktion nicht zuverlässig.

Der Zustand ist optimistisch: „Zuletzt gesendet“ bestätigt nur den erfolgreichen HTTP-Aufruf. Das bekannte Konfigurationsformat enthält keinen dokumentierten Istzustand. Vor dem ersten Aufruf und nach einem HA-/Bridge-Neustart ist der Zustand unbekannt; es wird beim Start nichts automatisch geschaltet. Ob die Lampe die Einstellung selbst über einen Stromausfall speichert, ist nicht verifiziert.

## Befehlsnachweis

Untersucht wurde die vom Nutzer bereitgestellte Android-APK **3.1.93** mit SHA-256:

`bf4c837951a1006f01f0d65afd0c0841250f9cf027ce4f494fac6da59e7bcd73`

- `assets/widget/script/topRightBtns_light.js`: Menüeintrag `PilotLamp` ruft `options.mcuLed` auf.
- `assets/widget/script/public.js`, Funktion `mcuLed`: explizite Befehle `mculed=off` und `mculed=on`.
- Derselbe Quelltext, Funktion `SetDevOrder`: lokaler Transport über `serverUrl + '?' + order`. Die App verwendet je nach Gerätegeneration alternativ MQTT.

Die Integration verwendet nur den lokalen HTTP-Weg. Vor dem Schreiben wird `read=config` abgefragt und die Seriennummer geprüft. Der Aufruf läuft mit derselben Anfrage-Sperre wie die Lichtsteuerung, verwendet aber kein `saveconfig` und verändert weder Kanäle noch Programme. Der Drehregler-/ButtonAction-Parameter ist eine andere Funktion.

Der Befehl ist damit im Hersteller-Code belegt; die konkrete physische Wirkung und Firmware-Kompatibilität sind noch am Gerät zu bestätigen. APK und entschlüsselte Herstellerdateien werden nicht im Repository veröffentlicht.
