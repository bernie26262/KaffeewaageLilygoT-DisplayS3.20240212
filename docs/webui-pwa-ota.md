# WebUI, PWA und OTA

## WebUI-Struktur

Die Haupt-WebUI ist aktuell in `src/coffee_web.cpp` eingebettet. Das ist historisch gewachsen und soll kurzfristig stabil bleiben.

Die PWA-/Homescreen-Icons und das Favicon liegen als Quelldateien unter:

```text
assets/kaffeewaage-192.png
assets/kaffeewaage-512.png
assets/kaffeewaage.ico
```

PlatformIO bettet diese Dateien ueber `board_build.embed_files` direkt in die read-only `.rodata`-Sektion der Firmware ein. Ein SPIFFS-/LittleFS-Dateisystem wird fuer die WebUI nicht mehr benoetigt.


## WebUI-Asset-Split

Die Haupt-WebUI wird weiterhin aus der Firmware/PROGMEM ausgeliefert. Seit dem Performance-Split besteht sie aber nicht mehr aus einer einzigen grossen HTML-Antwort. Stattdessen werden die grossen Bestandteile ueber getrennte Routen bereitgestellt:

```text
/           HTML-Grundgeruest
/coffee.css Stylesheet
/coffee_core.js   JavaScript: Basisfunktionen, Navigation, Overlays
/coffee_render.js JavaScript: Wartung, Stopwatch, State-Rendering
/coffee_events.js JavaScript: WebSocket-Verbindung und Event-Handler
```

Die einzelnen HTTP-Antworten der Haupt-WebUI bleiben dadurch klein. Das hat die Auslieferung der WebUI auf der Kaffeewaage stabilisiert und stark beschleunigt. Nach weiteren Tests wurde der JavaScript-Teil nochmals in drei kleinere Dateien aufgeteilt, damit kein einzelnes JS-Asset mehr ca. 39 kB gross ist.

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

Die binaeren PWA-/Browser-Assets werden zusammen mit der Haupt-WebUI durch dasselbe `firmware.bin` aktualisiert.

## Shot-Waage und Live-Graph

Die zweite Hauptseite ist keine manuelle Stoppuhr mehr, sondern die Shot-Waage. Der kompakte aktuelle Zustand kommt ueber `/ws` und enthaelt unter anderem:

- Session-ID und Zustand (`idle`, `armed`, `running`, `completed`)
- Shot-Zeit
- aktuelles, Spitzen- und Endgewicht
- aktuelle Flowrate
- Messpunktanzahl und Buffer-Vollstatus

Die Messreihe selbst wird nicht in jedes WebSocket-Paket eingebettet. Der Browser laedt sie inkrementell ueber:

```text
GET /api/shot/samples?session=<id>&from=<index>
```

Eine Antwort liefert maximal 160 neue Punkte. Jeder Punkt hat die kompakte Form:

```json
[zeit_ms, gewicht_g, flow_g_s]
```

Die Antwort ist mit `Cache-Control: no-store` markiert und enthaelt zusaetzlich Session-ID, Startindex, Gesamtanzahl sowie Running-/Completed-Status. Die Session-ID verhindert, dass Punkte zweier Shots vermischt werden.

Der responsive Canvas-Graph zeigt:

- Gewicht auf der linken Achse
- Flow in g/s auf der rechten Achse
- Zeit in Sekunden auf der unteren Achse

Beim wirklichen Start eines neuen Shots wird der alte Verlauf ersetzt. Tara allein loescht den letzten abgeschlossenen Verlauf nicht. Nach Seitenwechsel oder Browser-Neuladen kann der laufende beziehungsweise letzte Shot erneut aus dem RAM geladen werden. Nach einem ESP-Neustart ist der Verlauf weg.

## WebUI und lokales Display

WebUI-Aktionen synchronisieren weiterhin Modus und vorbereitete HMI-Seite, gelten aber nicht als lokale Aktivitaet. Sie duerfen deshalb weder das Backlight einschalten noch den Inaktivitaetstimer zuruecksetzen. Die lokale HMI-Seite wird erst beim naechsten echten Wakeup sichtbar.

## PWA-/Homescreen-Integration

Die WebUI enthaelt im HTML-`head` die relevanten PWA-/Mobile-Meta-Tags:

- Manifest-Link
- Favicon-Link
- Apple-Touch-Icon-Link
- Theme-Farbe
- Mobile-Web-App-Modus
- Apple-Mobile-Web-App-Modus
- Apple-App-Titel

Das Manifest wird als JSON aus der Firmware ueber folgende Route ausgeliefert:

```text
/manifest.json
```

Die Icons werden ueber stabile URLs bereitgestellt:

```text
/icon-192.png
/icon-512.png
/favicon.ico
```

Diese Routen liefern die direkt in die Firmware eingebetteten Binaerdaten aus.

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

Nach einem Firmware-Upload sollten diese URLs getestet werden:

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

Sie bietet genau einen Upload fuer `firmware.bin`. WebUI, Manifest und Icons sind Bestandteil dieses Firmware-Binaries.

Nach erfolgreichem Upload wird kein automatischer Neustart ausgefuehrt. Dadurch kann erst geprueft werden, ob der Upload erfolgreich war. Danach wird per Button neu gestartet.

## OTA-Dateinamen-Schutz

Die OTA-Seite prueft die ausgewaehlten Dateinamen doppelt:

1. clientseitig in der WebUI direkt bei der Dateiauswahl und erneut beim Upload-Klick
2. ESP-seitig im Upload-Handler, bevor `Update.begin(...)` aufgerufen wird

Erlaubter Dateiname:

| Upload-Feld | erlaubte Datei |
|---|---|
| Firmware | `firmware.bin` |

Bei falschem Dateinamen wird der Upload blockiert. ESP-seitig antwortet der Handler mit HTTP 400 und einer erklaerenden Meldung.

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

## Kein separates Dateisystem-Image

Ein `buildfs`-/`uploadfs`-Schritt ist nicht mehr erforderlich. Die Dateien unter `assets/` werden beim normalen Firmware-Build automatisch in `firmware.bin` eingebettet.
