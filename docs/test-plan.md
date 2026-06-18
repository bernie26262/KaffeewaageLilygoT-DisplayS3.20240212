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

## BLE-/Gaggiuino-Test

Voraussetzung: WebUI auf Shot-Waage umschalten und Gaggiuino mit `WeighMyBru` verbinden.

Pruefen:

- BLE startet nach ca. 5 Sekunden und Advertising ist aktiv
- WebUI und HMI zeigen den Bluetooth-Status korrekt
- Verbindung und Trennung erscheinen einmalig im RAM-Log
- Gewicht wird mit ungefaehr 5 Hz an die Maschine gemeldet
- TARE der Maschine tariert die Waage und bereitet die Shot-Erkennung vor
- Kommandos im Single-Dose-Modus werden ignoriert und entsprechend protokolliert
- redundante START-/STOP-Kommandos werden als `ohne Zustandsaenderung` protokolliert
- Gewichtspakete erzeugen keine laufenden Logzeilen

GaggiMate bleibt mangels verfuegbarer Testmaschine ausdruecklich ungetestet. Keine GaggiMate-spezifische Aenderung ohne spaeteren Praxistest freigeben.

## Shot-Session- und Graph-Test

- Tara auf der Shot-Seite zeigt `warte auf ersten Tropfen`
- ohne Tropfen endet die Bereitschaft nach 45 Sekunden
- erster sicher erkannter Tropfen startet Zeit und Graph automatisch
- Gewicht und Flow werden waehrend des Bezugs aktualisiert
- WebUI laedt neue Punkte inkrementell nach
- Shot endet nach dem relevanten Gewichtszuwachs automatisch
- finale Stillstandssekunden werden nicht an den Verlauf angehaengt
- Endgewicht, Zeit und Graph bleiben nach Shot-Ende sichtbar
- Wechsel auf andere WebUI-Seite und zurueck erhaelt den letzten Shot
- Browser-Neuladen erhaelt den letzten Shot aus dem RAM
- ein neuer Graph ersetzt den alten erst beim tatsaechlichen Shot-Start
- nach ESP-Neustart ist kein alter Shot-Verlauf mehr vorhanden

Direkter API-Test:

```text
http://<ip>/api/shot/samples?session=<session-id>&from=0
```

Erwartung: gueltiges JSON, maximal 160 Punkte pro Antwort und `Cache-Control: no-store`.

## Modus-, HMI- und Backlight-Test

Bei eingeschaltetem Display:

- WebUI Waage -> lokale Single-Dose-Seite vorbereitet
- WebUI Shot-Waage -> lokale Shot-Seite vorbereitet
- WebUI Daten -> Shot-Modus beendet und Daten-Uebersicht vorbereitet
- WebUI Einstellungen -> Shot-Modus beendet und nur Einstellungs-Uebersicht vorbereitet
- tiefere WebUI-Untertabs wechseln keine lokalen HMI-Unterseiten
- laufender Shot wird beim Verlassen sauber beendet
- wartende Shot-Erkennung wird beim Verlassen verworfen

Bei ausgeschaltetem Display:

- WebUI oeffnen/aktualisieren -> Display bleibt aus
- Hauptseite oder Einstellung ueber WebUI wechseln -> Display bleibt aus
- Tara und andere WebUI-Aktionen -> Display bleibt aus
- Display-Timeout ueber WebUI aendern -> Display bleibt aus und laufender Timeout wird nicht verlaengert
- BLE-Kommando -> Display bleibt aus
- Encoder/Taster lokal bedienen -> Display wacht auf
- Gewicht um mindestens 30 g aendern -> Display wacht auf

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
