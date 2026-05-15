# Testplan

## Build-Test

Nach jedem Diff:

```powershell
pio run -e KaffeewaageLilygoT-DisplayS3_20240212
```

Erwartung:

- Build ist gruen
- keine neuen Fehler
- neue Warnungen nur bewusst akzeptieren

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
pio run -e KaffeewaageLilygoT-DisplayS3_20240212 -t buildfs
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

- bekanntes Gefaess auflegen
- Auto-Tara abwarten
- Save-Freigabe pruefen
- Gefaess abheben
- Save wird gesperrt

Autodetect AUS:

- manuell tara ausloesen
- Save-Freigabe pruefen
- Dose speichern
- Statistikwerte pruefen

## TFT-/HMI-Beobachtung

Ein einmaliger Grafikfehler wurde beobachtet:

- Farben invertiert
- heller/weißer Hintergrund statt dunklem Design
- bisher nicht reproduzierbar

Aktueller Umgang:

- nur beobachten
- nicht ohne Reproduktion umbauen
- bei erneutem Auftreten Foto, Zeitpunkt und Bedienkontext notieren
