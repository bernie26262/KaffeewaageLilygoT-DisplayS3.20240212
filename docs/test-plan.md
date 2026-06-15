# Testplan

## Build-Test

Nach jedem Diff:

```powershell
pio run -e lilygo-t4-s3-lvgl
```

Erwartung:

- Build ist gruen
- keine neuen Fehler
- neue Warnungen nur bewusst akzeptieren

Hinweis: Für den aktuellen T4-S3-Branch nicht nur `pio run` verwenden, weil sonst je nach `platformio.ini` auch das Legacy-Environment gebaut werden kann.

## WebUI-Basistest

Im Browser oeffnen:

```text
http://<ip>/
```

Pruefen:

- Seite laedt
- WebSocket verbindet sich
- Gewicht wird aktualisiert
- Statusanzeige plausibel
- Zielgewicht kann eingegeben und gespeichert werden
- Buttons reagieren
- Log zeigt keine unerwarteten Fehler

## PWA-/Icon-Test

Direkte URLs pruefen:

```text
http://<ip>/manifest.json?v=99
http://<ip>/icon-192.png?v=99
http://<ip>/icon-512.png?v=99
http://<ip>/favicon.ico?v=99
```

Erwartung:

- Manifest liefert gueltiges JSON
- beide PNG-Icons werden angezeigt
- Favicon wird geladen
- Browser-Tab zeigt das Icon

Android-Test:

1. Seite in Chrome oeffnen
2. Menue oeffnen
3. Zum Startbildschirm hinzufuegen
4. Homescreen-Icon pruefen
5. App starten
6. Anzeige im Standalone-Modus pruefen

Bei Icon-Problemen:

- Browser-Cache umgehen, z.B. `?v=99`
- alte Homescreen-Verknuepfung loeschen
- neu hinzufuegen

## OTA-Test

Update-Seite oeffnen:

```text
http://<ip>/update
```

Firmware-Upload:

1. Firmware bauen
2. `firmware.bin` ueber Firmware-Upload hochladen
3. Erfolgsmeldung abwarten
4. per Button neu starten
5. WebUI wieder oeffnen

SPIFFS-Upload:

1. Dateisystem bauen

```powershell
pio run -e lilygo-t4-s3-lvgl -t buildfs
```

2. erzeugte SPIFFS-Binary ueber Dateisystem-Upload hochladen
3. Erfolgsmeldung abwarten
4. per Button neu starten
5. Icon-/Manifest-URLs pruefen


### OTA-Dateinamen-Schutz

Falsche Datei im Firmware-Feld:

1. `spiffs.bin` oder `littlefs.bin` im Firmware-Feld auswaehlen
2. Erwartung: Meldung `Firmware: falsche Datei gewaehlt. Erwartet: firmware.bin.`
3. Upload-Klick
4. Erwartung: Upload wird clientseitig blockiert

Falsche Datei im SPIFFS-Feld:

1. `firmware.bin` im SPIFFS-/Dateisystem-Feld auswaehlen
2. Erwartung: Meldung `SPIFFS: falsche Datei gewaehlt. Erwartet: spiffs.bin oder littlefs.bin.`
3. Upload-Klick
4. Erwartung: Upload wird clientseitig blockiert

Serverseitiger Schutz:

- Falls ein Upload die WebUI-Pruefung umgehen wuerde, muss der ESP mit HTTP 400 antworten.
- Firmware-Endpunkt akzeptiert nur `firmware.bin`.
- Dateisystem-Endpunkt akzeptiert nur `spiffs.bin` oder `littlefs.bin`.

## WebUI-Langlauf-Performance

Nach Änderungen an `src/coffee_web.cpp` oder den eingebetteten JS-Assets:

1. Browser-Konsole öffnen
2. WebUI mehrere Stunden offen lassen
3. Gewichtsanzeige und Wartungs-/Datenbereiche normal beobachten

Erwartung:

- keine dauernden Chrome-Meldungen `message handler took ... ms`
- WebSocket bleibt verbunden
- Gewichtsanzeige bleibt reaktiv
- Wartungs- und WLAN-Anzeigen aktualisieren weiter

Technischer Zielzustand: State-Nachrichten werden über `requestAnimationFrame` gebündelt und DOM-Schreibzugriffe werden nur bei tatsächlichen Änderungen ausgeführt.

## Storage-/Preferences-Test

Nach Storage-Aenderungen pruefen:

- gespeicherter Siebtraeger bleibt erhalten
- gespeichertes Zielgewicht bleibt erhalten
- Autodetect-Einstellung bleibt erhalten
- Gefaessgewichte bleiben erhalten
- Kalibrierwert bleibt erhalten
- Wartungs-/Statistikwerte bleiben erhalten

Kontrollsuche:

```powershell
Select-String -Path src\main.cpp -Pattern "preferences\.begin|preferences\.put|preferences\.get|preferences\.end" -CaseSensitive:$false
```

Erwartung: keine Treffer.

## Autodetect-/Save-Basistest

Autodetect EIN:

- HMI bewusst auf eine andere Seite stellen, z.B. Daten, Wartung oder WLAN
- WebUI auf Waage öffnen
- bekanntes Gefäß auflegen
- Auto-Tara abwarten
- Save-Freigabe prüfen
- Gefäß abheben
- Save wird gesperrt

Autodetect AUS:

- HMI bewusst auf eine andere Seite stellen
- über WebUI manuell Tara auslösen
- Save-Freigabe prüfen
- Dose speichern
- Statistikwerte prüfen
- Gewicht entfernen
- Save wird wieder gesperrt

Während Gefäß-Wizard, Kalibrier-Wizard oder Web-Wizard aktiv sind, muss Autodetect weiterhin pausiert bleiben.

## Wartung aktiv/inaktiv

Test je Wartungstyp Kaffeemaschine, Mühle und Filter:

1. Wartung aktiv lassen und Save Dose auslösen
2. Gesamtwerte und jeweilige Wartungszähler steigen
3. Wartung deaktivieren
4. Save Dose erneut auslösen
5. Gesamtwerte steigen weiter
6. Zähler der deaktivierten Wartung bleiben unverändert
7. WebUI zeigt bei Shots/Mahlgut `disabled`
8. HMI zeigt bei Shots/Mahlgut `-`
9. Wartung reaktivieren
10. Zähler laufen ab altem Stand weiter

## HMI-Gewichtsanzeige

- Kleine negative Werte zwischen `-0,05 g` und `0,0 g` dürfen nicht als `-0,0 g` erscheinen, sondern als `0,0 g`.
- Bei echten negativen Werten ab gerundet `-0,1 g` muss das Minuszeichen sichtbar bleiben.
- Leere Waage soll im Stable-Zustand ruhig bei `0,0 g` bleiben.
- Einzelne Bohnen mit etwas über `0,1 g` sollen eine sichtbare Änderung auslösen.
- Größere Gewichtsänderungen beim Auflegen/Abheben sollen schnell nachlaufen.

## TFT-/HMI-Beobachtung

Ein einmaliger Grafikfehler wurde beobachtet:

- Farben invertiert
- heller/weißer Hintergrund statt dunklem Design
- bisher nicht reproduzierbar

Aktueller Umgang:

- nur beobachten
- nicht ohne Reproduktion umbauen
- bei erneutem Auftreten Foto, Zeitpunkt und Bedienkontext notieren


## BLE-/Gaggiuino-Basistest

Monitor:

```powershell
pio device monitor -e lilygo-t4-s3-lvgl -b 115200 | Select-String "\[BLE\]|\[T4S3\]\[BLE\]|\[T4S3\]\[SHOT\]|Guru|abort|Brownout|Reboot"
```

Prüfen:

1. BLE startet etwa fünf Sekunden nach Boot.
2. Ohne Maschine: Status blau/`bereit`, Advertising aktiv.
3. Mit Maschine: Status grün/verbunden.
4. Disconnect startet Advertising erneut.
5. Tara von Gaggiuino wird auf der Shot-Seite ausgeführt.
6. Tara von Gaggiuino wird im Single-Dose-Modus ignoriert.
7. WLAN, WebUI und HX711 bleiben während BLE-Betrieb stabil.

Bekannte Einschränkung: Beim getesteten Gaggiuino-Client werden keine `START`, `STOP` oder `RESET`-Kommandos gesendet. Ein Aus-/Einschalten der Bluetooth-Scale-Funktion auf der Maschine kann auf Gaggiuino-Seite zu einem Neustart führen. Die Waage selbst zeigt dabei bisher keinen Guru, Abort oder Brownout.

## Shot-Automatik und Flowrate

### Tara ohne Shot

1. Shot-Seite öffnen.
2. Tara über HMI, WebUI oder Maschine auslösen.
3. Status zeigt `wartet auf Bezug`.
4. 45 Sekunden nichts auflegen/einlaufen lassen.
5. Erwartung: Armed endet, Session geht in Bereitschaft zurück.
6. Ein zuvor abgeschlossener Graph bleibt erhalten.

### Realer Shot

1. Tara auslösen.
2. Bezug starten.
3. Timer startet beim ersten bestätigten Flüssigkeitsgewicht.
4. Gewicht, Zeit und Flowrate laufen auf HMI/WebUI.
5. Nach Ende des relevanten Gewichtszuwachses stoppt die Session automatisch.
6. Endzeit, Endgewicht und Graph bleiben sichtbar.
7. Pre-Infusion vor dem ersten Tropfen ist bewusst nicht Teil dieser Zeit.

### Zweiter Shot

1. Nach abgeschlossenem Shot erneut Tara auslösen.
2. Vorheriger Graph bleibt während Armed sichtbar.
3. Erst beim echten Start des neuen Shots wird der alte Puffer überschrieben.

### Plausibilität Flowrate

- blaue Flowrate-Kurve soll den Trend zeigen und nicht jedem HX711-Zittern folgen
- Einschwingzeit von etwa 1 bis 2 Sekunden ist durch das 2,5-Sekunden-Regressionsfenster normal
- nach Stop wird der aktuelle Flow-Wert auf 0 gesetzt
- Gewichtsdaten an Gaggiuino bleiben von der Flowrate-Glättung unabhängig

## Shot-Graph / RAM-Reconnect

1. Shot durchführen und Graph abwarten.
2. Browser neu laden oder WebSocket kurz trennen.
3. Erwartung: letzter Shot wird über `/api/shot/samples` wieder aus RAM geladen.
4. Tara ohne folgenden Shot darf den Graph nicht löschen.
5. ESP-Neustart darf den Graph bewusst löschen.

## Mehrgeräte- und HMI-Seitensynchronisierung

1. HMI auf Shot stellen.
2. WebUI auf einem neuen Gerät öffnen.
3. Erwartung: neue WebUI öffnet nach erstem State Shot.
4. Desktop auf Daten wechseln.
5. Erwartung: HMI und weiteres Mobilgerät wechseln ebenfalls auf Daten.
6. Einstellungen-Unterseiten ebenfalls prüfen.
7. OTA-Seite öffnen und über `Zurück zur WebUI` zurückkehren.
8. Erwartung: aktuelle HMI-Seite wird wiederhergestellt.

## Produktiv-Cache

1. Nach einer Firmware mit neuer `COFFEE_WEB_ASSET_VERSION` WebUI einmal neu öffnen.
2. Desktop und Mobilgerät prüfen.
3. Danach Browser schließen und erneut öffnen.
4. Erwartung: Assets laden schnell aus Cache, WebSocket verbindet normal.
5. Bei späteren WebUI-Änderungen zentrale Asset-Version erhöhen.
6. Browserdaten dürfen im Normalfall nicht manuell gelöscht werden müssen.
