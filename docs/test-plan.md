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
