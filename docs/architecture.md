# Architekturüberblick

## Ziel

Die Kaffeewaage soll als eigenständiges ESP32-S3-Gerät funktionieren und gleichzeitig eine moderne WebUI bereitstellen. Der aktuelle Hauptstand ist der **T4-S3-/LVGL-Branch** mit lokalem Touch-HMI und WebUI. Der Legacy-TFT-/Encoder-Stand bleibt als Referenz im selben Repo erhalten.

## Branches und Build-Environments

| Bereich | Branch / Environment | Status |
|---|---|---|
| T4-S3 / LVGL | `feature/t4s3-lvgl-touch` / `lilygo-t4-s3-lvgl` | aktueller Hauptstand |
| Legacy-TFT | `feature/legacy-shot-scale` / `KaffeewaageLilygoT-DisplayS3_20240212` | finaler klassischer Stand mit Gaggiuino-Shot-Waage / Referenz |

Für T4-S3 immer gezielt bauen:

```powershell
pio run -e lilygo-t4-s3-lvgl
```

Ein nacktes `pio run` kann zusätzlich das Legacy-Environment bauen und dadurch Fehler zeigen, die für den T4-S3-Stand nicht relevant sind.

## Hauptmodule T4-S3

### `src/t4s3_main.cpp`

Zentrale T4-S3-Hardware- und Ablaufsteuerung:

- LilyGO-AMOLED-Initialisierung
- LVGL-Tick, Display-Flush, Touch-Eingabe
- HX711-Initialisierung und Messwertverarbeitung
- adaptive Gewichtsanzeige
- Display-Sleep/Wakeup
- AppState-Befüllung für WebUI
- Start von WLAN, WebUI und OTA

### `src/ui_t4s3/`

LVGL-HMI für den T4-S3:

- Waage
- Shot-Waage mit Gewicht, Zeit und Flowrate
- Daten
- Wartung
- WLAN / Netzwerk
- System / OTA
- Kalibrier- und Gefäß-Wizard

Wichtig: Der Save-Button wird im inaktiven Zustand bewusst **nicht** über `LV_STATE_DISABLED` dargestellt. Der LVGL-Disabled-State hellt auf dem AMOLED stark auf. Stattdessen setzt die UI eigene dunkle Farben und schaltet nur `LV_OBJ_FLAG_CLICKABLE`.

### `src/t4s3_settings.cpp/.h`

Persistente T4-S3-HMI- und Waagenwerte im Namespace `t4s3ui`:

- Bildschirmtimeout
- Autodetect
- ausgewählter Siebträger
- Sollgewichte und Namen
- Gefäßgewichte
- Statistik-/Wartungswerte
- Wartungszeiten und Wartung aktiv/inaktiv
- HX711-Kalibrierfaktor

### `src/t4s3_wifi.cpp/.h`

T4-S3-nahe WLAN-Status- und Hilfslogik.

### `src/t4s3_weight_diag.cpp/.h`

USB-unabhängiger Diagnose-Recorder für die komplette T4-S3-Gewichtskette:

- 10-Hz-Aufzeichnung in einem PSRAM-Ringpuffer
- bevorzugt 9.000 Samples / 15 Minuten, Fallback 1.200 Samples / 2 Minuten
- Library-, Median-, FAST-, STABLE-, DISPLAY- und SHOT-Werte
- Flow, Moving/Stable und Shot-State
- Ereignisse für Tara, Autotara, BLE, Shot-Start/-Stop/-False-Start
- Steuerung und CSV-Export über die Home-WebUI

Details: `docs/weight-diagnostics.md`.

### `src/app_state.h`

Gemeinsamer Zustand für WebUI/JSON-Ausgabe. `t4s3_main.cpp` aktualisiert den AppState; `coffee_web.cpp` serialisiert ihn für WebSocket-Clients.

Der AppState transportiert zusätzlich:

- aktiven Waagenmodus (`single_dose` / `shot`)
- aktive HMI-Hauptseite und Einstellungs-Unterseite
- BLE-Status
- Shot-Session-Status einschließlich Flowrate und Sampleanzahl

Das lokale HMI ist Master für den Seitenzustand. Ein neu verbundener WebUI-Client übernimmt nach dem ersten WebSocket-State die aktuelle HMI-Seite, ohne beim Initialisieren einen konkurrierenden Seitenbefehl zurückzusenden.

### `src/coffee_web.cpp/.h`

Gemeinsame WebUI- und WebSocket-Schicht für Legacy und T4-S3:

- eingebettete WebUI
- getrennte Asset-Routen `/coffee.css`, `/coffee_core.js`, `/coffee_render.js`, `/coffee_events.js`
- WebSocket-Route `/ws`
- WebSocket-Kommandos
- JSON-State-Erzeugung
- PWA-Manifest-Handler
- Icon-/Favicon-Routen
- Shot-Sample-Endpunkt `/api/shot/samples`
- kompakte `shot_telemetry`-WebSocket-Nachrichten während `armed/running`
- Weight-Diagnose-Endpunkte für Liveansicht und CSV-Export
- zentrale Asset-Versionierung und Produktiv-Caching
- Synchronisierung mehrerer WebUI-Clients mit dem HMI-Seitenzustand

Das WebUI-Rendering ist browserseitig optimiert: Full-State-Nachrichten werden per `requestAnimationFrame` gebündelt, und DOM-Werte werden nur bei Änderung geschrieben. Außerhalb eines Shots wird der Full State alle `500 ms` gesendet. Während `armed/running` läuft ein kleiner `shot_telemetry`-Pfad mit `200 ms`, während der große Full State auf `1000 ms` reduziert wird. Das verbessert Gewicht, Flow und Timer auf der Shot-Seite, ohne den vollständigen AppState fünfmal pro Sekunde zu übertragen.


### `src/ble_scale.cpp/.h`

WeighMyBru-kompatible BLE-Peripheral-Schicht:

- Gerätename `WeighMyBru` für Gaggiuino-Erkennung
- Nordic-UART-artiger Service `6E400001-...`
- Gewichtspakete mit 5 Hz
- Kommando-Characteristic für `TARE`, `START`, `STOP` und `RESET`
- Advertising-Neustart nach Disconnect
- BLE-Start fünf Sekunden nach Boot, damit Display, WLAN und BLE-Koexistenz stabil bleiben

Im aktuell getesteten Gaggiuino-Client wird nur `TARE` gesendet. Die Unterstützung der übrigen Kommandos bleibt für andere Clients beziehungsweise spätere Firmwarestände erhalten.

### `src/shot_session.cpp/.h`

Zentrale, nicht persistente Shot-Session:

- Zustände `Idle`, `Armed`, `Running`, `Completed`
- Arming durch Tara im Shot-Modus
- Startkandidat ab `0,25 g`, Bestätigung ab `0,45 g`
- zusätzliche Trend-Plausibilisierung über ca. `1,2 s`: mindestens `0,15 g` Nettozuwachs, mindestens `0,08 g/s` Steigung und maximal `0,05 g` RMSE zum linearen Trend
- Armed-Timeout nach 45 Sekunden
- False-Start-Watchdog für frühe, leichte und flowfreie Sessions, die wieder nahe Null zurückfallen
- Recovery durch Gaggiuino-Tara bei einem sehr frühen, kleinen Fehlstart
- automatischer Stop nach ausbleibendem relevantem Gewichtszuwachs
- maximal 1.200 Samples bei 10 Hz, entsprechend 120 Sekunden
- letzter Shot bleibt bis zum tatsächlichen Start des nächsten Shots im RAM
- Flowrate über lineare Regression eines 2,5-Sekunden-Fensters plus EMA-Glättung

Die Shot-Zeit beginnt beim ersten **softwareseitig plausibilisierten Flüssigkeitszuwachs**. Sie ist deshalb nicht identisch mit Pumpenzeit/Pre-Infusion und kann bei sehr langsamem ersten Tropfen etwas nach dem optisch sichtbaren ersten Tropfen starten.

### `src/coffee_ota.cpp/.h`

Update-Seite `/update` mit getrenntem Upload für:

- Firmware (`firmware.bin`)
- Dateisystem (`spiffs.bin` oder `littlefs.bin`)

Der ESP rebootet nach Upload nicht automatisch. Der Neustart erfolgt per Button. Falsche Dateinamen werden client- und serverseitig abgelehnt.

### `src/coffee_wifi.cpp/.h`

WLAN-Provisioning und gespeicherte WLAN-Daten:

- Laden/Speichern/Löschen von SSID und Passwort in NVS
- Auswahl zwischen gespeicherten WLAN-Daten und Standard-WLAN aus Firmware
- Setup-Access-Point `Waagen-Setup`
- Setup-Seite unter `http://192.168.4.1/`

### `src/coffee_storage.cpp/.h`

Legacy-kompatible Storage-Schicht für klassische Werte und WebUI-Funktionen. Für T4-S3-spezifische HMI-Werte wird zusätzlich `t4s3_settings` verwendet.


## Betriebsarten Single Dose und Shot

Die Betriebsart wird zentral aus der aktiven HMI-Hauptseite abgeleitet. BLE bleibt in beiden Modi initialisiert und verbunden.

### Single Dose

- Autodetect, Gefäßerkennung, Auto-Tara und Save-ready aktiv
- Maschinenkommandos über BLE werden nicht ausgeführt
- schneller adaptiver Anzeigeweg für Bohnen und Gefäßwechsel

### Shot-Waage

- Autodetect und Single-Dose-Save-Logik vollständig deaktiviert
- Tara lokal oder von der Maschine möglich
- BLE-Maschinenkommandos freigegeben
- eigener ruhiger Shot-Gewichtspfad
- automatische Zeitmessung ab erstem plausibilisierten Flüssigkeitszuwachs
- Flowrate und RAM-Verlauf für HMI/WebUI

Ein Seitenwechsel beendet die BLE-Verbindung nicht. Dadurch werden unnötige Reconnects vermieden.

## Waagenlogik und Autodetect

Autodetect, Auto-Tara und Save-ready sind nicht mehr an die aktuell sichtbare HMI-Seite gekoppelt. Dadurch funktioniert die Waagenlogik auch dann, wenn die WebUI auf der Waage-Seite benutzt wird und das lokale HMI gerade Daten, Wartung, WLAN oder System zeigt.

Für die Responsiveness wartet die Gefäßerkennung nicht mehr auf das globale Stable-Flag. Während Bewegung und Settling folgt die Anzeige dem FAST-Pfad; ein erkanntes Gefäß muss `800 ms` innerhalb der Erkennungstoleranz bleiben, bevor Auto-Tara ausgeführt wird. Das Abheben eines bereits auto-tarierten Gefäßes bleibt dagegen zusätzlich an `stable` gekoppelt, damit kurze negative Störungen keine Leer-Tara auslösen.

Beim mechanischen Endaufbau ist darauf zu achten, dass kein HX711-/Wägezellenkabel die Wägeplatte berührt. Ein solcher Kontakt hatte im Test einen spiegelbildlichen Nachlauf von etwa `+0,2…0,3 g` nach Belastung und entsprechend negativ nach Entlastung erzeugt. Nach Freilegen des Kabels blieb der Nullpunkt über mehrere Zyklen stabil.

Bewusst blockiert bleibt Autodetect während Sonderabläufen:

- Gefäß-Wizard
- Kalibrier-Wizard
- Web-Wizard / Web-Messdialoge
- sonstige Mess- und Verwaltungsabläufe, bei denen Auto-Tara stören würde

## Wartung

Wartung aktiv/inaktiv ist fachlich von den gespeicherten Wartungszeitpunkten getrennt:

- Deaktiviert: keine Warnung, Zeitpunkt bleibt gespeichert, zugehörige Shots-/Mahlgut-Zähler frieren ein.
- Aktiviert: Warnung erscheint sofort wieder, falls die Wartung nach gespeichertem Zeitpunkt eigentlich fällig ist; Zähler laufen ab altem Stand weiter.

WebUI zeigt bei deaktivierter Wartung `disabled`, das HMI wegen Platz `-`.

## Grundsätze für weitere Refaktorierung

- Kleine Diffs bevorzugen
- Build nach jedem Schritt
- T4-S3 mit `pio run -e lilygo-t4-s3-lvgl` bauen
- WebUI nur vorsichtig anfassen und Langlauf im Browser testen
- keine direkte neue Preferences-Logik in großen UI-Dateien, sondern Storage-/Settings-Funktionen nutzen
- Legacy und T4-S3 nicht blind gegenseitig überschreiben; Änderungen gezielt portieren
