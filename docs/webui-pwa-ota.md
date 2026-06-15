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
## WebSocket-Rendering / Langlauf-Performance

Die WebUI verarbeitet Live-State-Nachrichten über WebSocket. Nach langen Laufzeiten traten im Browser zeitweise Chrome-Meldungen wie diese auf:

```text
[Violation] 'message' handler took 315ms
```

Der aktuelle Stand reduziert die Last im Browser:

- eingehende State-Nachrichten werden über `requestAnimationFrame` gebündelt
- wenn mehrere States schnell nacheinander eintreffen, wird nur der letzte State gerendert
- Text und HTML werden nur ins DOM geschrieben, wenn sich der Inhalt geändert hat
- teure Listen wie Wartung, Gefäße, Siebträger und WLAN-Balken werden über Signaturen gecacht

Der Fix wurde auf dem T4-S3 nach mehreren Stunden Laufzeit ohne neue `message handler`-Violations getestet.


## Shot-Graph und Sample-Endpunkt

Die Shot-Seite zeigt den laufenden oder zuletzt abgeschlossenen RAM-Shot mit zwei Kurven:

- Gewicht in Gramm
- Flowrate in g/s

Die normalen WebSocket-State-Nachrichten enthalten nur Status, aktuelle Werte, Session-ID und Sampleanzahl. Die Messpunkte werden paketweise über folgenden Endpunkt nachgeladen:

```text
/api/shot/samples?from=<index>&limit=120
```

Eigenschaften:

- maximal 1.200 Punkte bei 10 Hz
- nur laufender beziehungsweise letzter Shot
- kein Flash- oder NVS-Logging
- Browser-Reconnect lädt den vorhandenen RAM-Verlauf erneut
- neuer Verlauf beginnt erst beim tatsächlichen Shot-Start
- Tara ohne Shot und Armed-Timeout löschen den vorherigen Verlauf nicht

## Seitensynchronisierung

Das lokale HMI ist Master für Hauptseite und Einstellungs-Unterseite. Der AppState liefert `ui_page` und `ui_settings_panel`.

Ein neuer WebUI-Client wartet auf den ersten WebSocket-State und öffnet danach die aktuelle HMI-Seite, ohne beim initialen Abgleich einen eigenen Navigationsbefehl zurückzusenden. Spätere Bedienwechsel werden über HMI/AppState an alle Clients verteilt.

Das gilt auch bei der Rückkehr von der OTA-Seite. Der Link führt schlicht auf `/`; nach dem WebSocket-Verbindungsaufbau erscheint wieder die aktuelle HMI-Seite.

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

## Cache-Busting und Produktiv-Caching

CSS und JavaScript verwenden eine gemeinsame zentrale Versionskennung in `src/coffee_web.cpp`:

```cpp
#define COFFEE_WEB_ASSET_VERSION "20260614b"
```

Die Versionskennung wird automatisch an `/coffee.css` und alle JavaScript-Routen angehängt. Bei jeder Änderung an eingebettetem HTML, CSS oder JavaScript muss ausschließlich diese zentrale Kennung erhöht werden.

Produktivstrategie:

| Ressource | Cache-Control |
|---|---|
| HTML `/` | `no-store` |
| CSS/JavaScript | `public, max-age=31536000, immutable` |
| Manifest | `no-cache, max-age=0, must-revalidate` |
| Icons/Favicon | `public, max-age=31536000, immutable` |
| Shot-Samples | `no-store` |

Damit werden unveränderte Assets schnell aus dem Browsercache geladen. Nach einer Versionsänderung entstehen neue URLs, sodass kein Gemisch aus neuem HTML und altem JavaScript mehr auftreten sollte.

Bei Icon-Änderungen zusätzlich die Icon-Version im HTML erhöhen. Android aktualisiert Homescreen-Icons häufig erst nach Löschen und erneutem Anlegen der Verknüpfung.

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
pio run -e lilygo-t4-s3-lvgl
```

Die erzeugte Firmware-Binary liegt unter:

```text
.pio\build\KaffeewaageLilygoT-DisplayS3_20240212\firmware.bin
```

## SPIFFS bauen

Bei OTA-Projekten nicht `pio run -t uploadfs` verwenden.

Stattdessen:

```powershell
pio run -e lilygo-t4-s3-lvgl -t buildfs
```

Danach das erzeugte SPIFFS-/LittleFS-Image aus dem Build-Ordner über `/update` hochladen.
