# WebUI, PWA und OTA

## WebUI-Struktur

Die Haupt-WebUI ist aktuell in `src/coffee_web.cpp` eingebettet. Das ist historisch gewachsen und soll kurzfristig stabil bleiben.

Die PWA-/Homescreen-Icons liegen unter `assets/` und werden mittels `board_build.embed_files` in die Firmware eingebettet:

```text
assets/kaffeewaage-192.png
assets/kaffeewaage-512.png
assets/kaffeewaage.ico
```

Die T4-S3 benötigt **kein SPIFFS-/LittleFS-Dateisystem** zum Ausliefern ihrer WebUI.


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

Die Haupt-WebUI und die eingebetteten PWA-Icons werden alle aus der Firmware geliefert; es gibt keinen separaten Filesystem-OTA-Upload.
## WebSocket-Rendering / Langlauf-Performance

Die WebUI verarbeitet Live-State-Nachrichten über WebSocket. Nach langen Laufzeiten traten im Browser zeitweise Chrome-Meldungen wie diese auf:

```text
[Violation] 'message' handler took 315ms
```

Der aktuelle Stand reduziert die Last im Browser:

- eingehende Full-State-Nachrichten werden über `requestAnimationFrame` gebündelt
- wenn mehrere States schnell nacheinander eintreffen, wird nur der letzte State gerendert
- Text und HTML werden nur ins DOM geschrieben, wenn sich der Inhalt geändert hat
- teure Listen wie Wartung, Gefäße, Siebträger und WLAN-Balken werden über Signaturen gecacht
- während `armed/running` kommt zusätzlich ein kleines `shot_telemetry`-Paket alle `200 ms`
- in dieser Phase sinkt die Rate des großen Full State von `500 ms` auf `1000 ms`

Der Fix wurde auf dem T4-S3 nach mehreren Stunden Laufzeit ohne neue `message handler`-Violations getestet. Die zusätzliche Shot-Telemetrie wurde anschließend mit realen Shots geprüft und erzeugt eine sichtbar flüssigere Gewicht-/Flow-/Timer-Darstellung.


## Shot-Graph und Sample-Endpunkt

Die Shot-Seite zeigt den laufenden oder zuletzt abgeschlossenen RAM-Shot mit zwei Kurven:

- Gewicht in Gramm
- Flowrate in g/s

Der normale Full State enthält den vollständigen App-Zustand. Während `armed/running` wird zusätzlich `shot_telemetry` gesendet. Das kompakte Paket enthält nur für den Shot relevante Felder wie Session-ID, Zustand, Zeit, aktuelles Gewicht, Peak, Flow und Sampleanzahl.

Die Messpunkte selbst werden weiterhin paketweise über folgenden Endpunkt nachgeladen:

```text
/api/shot/samples?from=<index>&limit=120
```

Eigenschaften:

- maximal 1.200 Punkte bei 10 Hz
- Shot-Live-Telemetrie ca. 5 Hz (`200 ms`), Full State während Shot ca. 1 Hz
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

Diese Routen liefern die direkt eingebetteten Flash-Assets, ohne Mounten eines Dateisystems.

## Cache-Busting und Produktiv-Caching

CSS und JavaScript verwenden eine gemeinsame zentrale Versionskennung in `src/coffee_web.cpp`:

```cpp
#define COFFEE_WEB_ASSET_VERSION "20261009wifiB2"
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

Nach dem Firmware-Upload sollten diese URLs getestet werden:

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

## OTA-Seite und Ablauf

Die separat in `src/coffee_ota.cpp` eingebettete OTA-Seite unter `http://<ip>/update` verwendet das dunkle WebUI-Design und bietet **nur Firmware-OTA**. Erlaubt ist `firmware.bin` aus:

```text
.pio\build\lilygo-t4-s3-lvgl\firmware.bin
```

Der Ablauf ist bewusst in Zustände aufgeteilt:

| Zustand | Verhalten |
|---|---|
| Keine / falsche Datei | Upload-Button deaktiviert, bei falschem Namen Fehlermeldung |
| Gültige `firmware.bin` | Upload-Button grün und aktiv |
| Upload läuft | Fortschrittsanzeige, deutliche Warnung, Dateiauswahl und Navigation gesperrt, `beforeunload` aktiv |
| Upload erfolgreich | Fortschritt 100 %, weiterer Upload gesperrt, nur Neustart möglich; „Zurück zur WebUI“ zunächst ausgeblendet |
| Neustart angefordert | Verbindungsabbruch erwartet; Hinweis erscheint und „Zurück zur WebUI“ wird wieder angeboten |
| ESP32 neu gestartet | OTA wieder verfügbar; WebUI, WLAN und BLE normal nutzbar |

Beim Beginn eines **gültigen** Firmware-Uploads wird BLE mit `coffeeBleScaleStopForOta()` gestoppt und bis zum Neustart nicht mehr gestartet. Bei falschem Dateinamen bleibt BLE unverändert.

### Serverseitiger OTA-Schutz

- `POST /update/firmware`: validiert den Dateinamen vor `Update.begin(...)`; ungültige Dateinamen werden mit HTTP 400 abgelehnt.
- `Update.end(true)` entscheidet, ob der Upload wirklich erfolgreich war. Erst dann wird `otaFirmwareReadyForReboot` gesetzt.
- `GET /update/status`: JSON `{"reboot_required":true|false}`; nach erfolgreichem Upload bleibt die Sperre auch nach `Strg+F5` aktiv.
- Ein erneuter Upload nach Erfolg wird bis zum Neustart serverseitig mit HTTP 409 abgewiesen.
- `POST /update/reboot`: fordert den kontrollierten ESP32-Neustart an.
- Unterbrochener Upload: Vor einem neuen Versuch wird bei `Update.isRunning()` der unvollständige Zustand mit `Update.abort()` verworfen. Ein neuer Upload beginnt vollständig bei **0 %**; echtes Resume ab einer Byteposition existiert nicht.

**Teststatus:** Normale Firmware-Uploads, BLE-Abschaltung, Neustart, Dateinamenprüfung und Sperre nach Erfolg wurden auf Hardware bestätigt. Ein **absichtlich unterbrochener Upload** wurde bislang nur per Code-/Simulationstest geprüft, nicht auf Hardware provoziert.

## Firmware bauen und hochladen

```powershell
pio run -e lilygo-t4-s3-lvgl
```

Anschließend `.pio\build\lilygo-t4-s3-lvgl\firmware.bin` über `/update` auswählen, hochladen und nach Erfolg „ESP32 jetzt neu starten“ drücken. Ein zusätzliches `buildfs`, `uploadfs`, `spiffs.bin` oder `littlefs.bin` ist für die T4-S3 **nicht erforderlich**; die Filesystem-OTA-Sektion wurde entfernt, die Partitionstabelle bleibt unverändert.

Die doppelte Dateinamenprüfung wird in `docs/ota_filename_validation.md` beschrieben.
