# WebUI, PWA und OTA

## WebUI-Struktur

Die Haupt-WebUI ist aktuell in `src/coffee_web.cpp` eingebettet. Das ist historisch gewachsen und soll kurzfristig stabil bleiben.

Bereits ausgelagert ins SPIFFS-Dateisystem sind die PWA-/Homescreen-Icons:

```text
data/kaffeewaage-192.png
data/kaffeewaage-512.png
data/kaffeewaage.ico
```

## PWA-/Homescreen-Integration

Die WebUI enthaelt im HTML-`head` die relevanten PWA-/Mobile-Meta-Tags:

- Manifest-Link
- Favicon-Link
- Apple-Touch-Icon-Link
- Theme-Farbe
- Mobile-Web-App-Modus
- Apple-Mobile-Web-App-Modus
- Apple-App-Titel

Das Manifest wird derzeit nicht als Datei aus `data/` ausgeliefert, sondern als JSON aus der Firmware ueber:

```text
/manifest.json
```

Die Icons werden ueber stabile URLs bereitgestellt:

```text
/icon-192.png
/icon-512.png
/favicon.ico
```

Diese Routen mappen auf die Dateien im SPIFFS.

## Cache-Busting

Die WebUI verwendet Versionsparameter an PWA-/Icon-URLs, z.B.:

```text
/manifest.json?v=1
/icon-192.png?v=1
/icon-512.png?v=1
/favicon.ico?v=1
```

Bei Icon-Aenderungen kann die Version erhoeht werden, damit Browser und Android nicht alte Icons weiterverwenden.

Android aktualisiert Homescreen-Icons oft nicht zuverlaessig nachtraeglich. Bei Icon-Aenderungen daher alte Homescreen-Verknuepfung loeschen und neu anlegen.

## Pruef-URLs

Nach Firmware- und SPIFFS-Upload sollten diese URLs getestet werden:

```text
http://<ip>/manifest.json?v=99
http://<ip>/icon-192.png?v=99
http://<ip>/icon-512.png?v=99
http://<ip>/favicon.ico?v=99
```

Erwartung:

- Manifest liefert JSON
- Icons liefern Bilddaten
- Favicon erscheint im Browser-Tab
- Android Chrome kann die Seite als Homescreen-App installieren

## OTA-Seite

Die OTA-Seite ist erreichbar unter:

```text
http://<ip>/update
```

Sie bietet getrennte Uploads fuer:

- Firmware
- SPIFFS-Dateisystem

Nach erfolgreichem Upload wird kein automatischer Neustart ausgefuehrt. Dadurch kann erst geprueft werden, ob der Upload erfolgreich war. Danach wird per Button neu gestartet.

## OTA-Dateinamen-Schutz

Die OTA-Seite prueft die ausgewaehlten Dateinamen doppelt:

1. clientseitig in der WebUI direkt bei der Dateiauswahl und erneut beim Upload-Klick
2. ESP-seitig im Upload-Handler, bevor `Update.begin(...)` aufgerufen wird

Erlaubte Dateinamen:

| Upload-Feld | erlaubte Datei |
|---|---|
| Firmware | `firmware.bin` |
| SPIFFS/Dateisystem | `spiffs.bin` oder `littlefs.bin` |

Bei falschem Dateinamen wird der Upload blockiert. ESP-seitig antwortet der Handler mit HTTP 400 und einer erklaerenden Meldung. Dadurch wird verhindert, dass versehentlich `spiffs.bin` als Firmware oder `firmware.bin` als Dateisystem-Image geschrieben wird.

Details und Testfaelle stehen in:

```text
docs/ota_filename_validation.md
```


## Firmware bauen

```powershell
pio run -e KaffeewaageLilygoT-DisplayS3_20240212
```

Die erzeugte Firmware-Binary liegt unter:

```text
.pio\build\KaffeewaageLilygoT-DisplayS3_20240212\firmware.bin
```

## SPIFFS bauen

Bei OTA-Projekten nicht `pio run -t uploadfs` verwenden.

Stattdessen:

```powershell
pio run -e KaffeewaageLilygoT-DisplayS3_20240212 -t buildfs
```

Danach das erzeugte SPIFFS-Image aus dem Build-Ordner ueber `/update` hochladen.
