# WebUI, PWA und OTA

## WebUI-Struktur

Die Haupt-WebUI ist aktuell in `src/coffee_web.cpp` eingebettet. Das ist historisch gewachsen und soll kurzfristig stabil bleiben.

Bereits ausgelagert ins SPIFFS-Dateisystem sind die PWA-/Homescreen-Icons:

```text
data/kaffeewaage-192.png
data/kaffeewaage-512.png
data/kaffeewaage.ico
```


## WebUI-Asset-Split

Die Haupt-WebUI wird weiterhin aus der Firmware/PROGMEM ausgeliefert. Seit dem Performance-Split besteht sie aber nicht mehr aus einer einzigen grossen HTML-Antwort. Stattdessen werden die grossen Bestandteile ueber getrennte Routen bereitgestellt:

```text
/           HTML-Grundgeruest
/coffee.css Stylesheet
/coffee_core.js   JavaScript: Basisfunktionen, Navigation, Overlays
/coffee_render.js JavaScript: Wartung, Stopwatch, State-Rendering
/coffee_events.js JavaScript: WebSocket-Verbindung und Event-Handler
```

Dadurch bleibt der Betrieb weiterhin ohne zusaetzlichen SPIFFS-/LittleFS-Upload fuer die Haupt-WebUI moeglich, aber die einzelnen HTTP-Antworten sind deutlich kleiner. Das hat die Auslieferung der WebUI auf der Kaffeewaage stabilisiert und stark beschleunigt. Nach weiteren Tests wurde der JavaScript-Teil nochmals in drei kleinere Dateien aufgeteilt, damit kein einzelnes JS-Asset mehr ca. 39 kB gross ist.

Gemessene Referenzwerte nach dem Split:

```text
/           ca. 12,7 kB
/coffee.css ca. 12,6 kB
/coffee_core.js   ca. 15 kB
/coffee_render.js ca. 10 kB
/coffee_events.js ca. 14 kB
```

Zum Testen nach Firmware-Upload koennen Cache-Buster verwendet werden:

```text
http://<ip>/?v=test
http://<ip>/coffee.css?v=test
http://<ip>/coffee_core.js?v=test
http://<ip>/coffee_render.js?v=test
http://<ip>/coffee_events.js?v=test
```

Wichtig: Die PWA-/Homescreen-Icons bleiben weiterhin im SPIFFS-Dateisystem. Der Asset-Split betrifft nur die Haupt-WebUI aus `src/coffee_web.cpp`.

## WebSocket-Rendering und Langzeitbetrieb

Die WebUI bekommt regelmaessig JSON-State-Nachrichten ueber `/ws`. Nach vielen Erweiterungen wurde das Rendering entlastet, damit der Browser-Hauptthread auch nach langer Laufzeit nicht blockiert:

- State-Nachrichten werden mit `requestAnimationFrame` gebuendelt.
- Wenn mehrere State-Nachrichten vor dem naechsten Frame eintreffen, wird nur der neueste State gerendert.
- Text- und HTML-Inhalte werden nur ins DOM geschrieben, wenn sich der Inhalt geaendert hat.
- Teurere Listenbereiche wie Wartung, Gefaesse, Siebtraeger und WLAN-Balken werden nicht bei jedem State komplett neu aufgebaut.

Referenzproblem in Chrome:

```text
coffee_events.js ... [Violation] 'message' handler took ... ms
```

Nach Aenderungen an `coffee_web.cpp` sollte die WebUI mehrere Stunden offen bleiben und die Konsole beobachtet werden.
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
