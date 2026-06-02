# Architekturüberblick

## Ziel

Die Kaffeewaage soll als eigenständiges ESP32-S3-Gerät funktionieren und gleichzeitig eine moderne WebUI bereitstellen. Der aktuelle Hauptstand ist der **T4-S3-/LVGL-Branch** mit lokalem Touch-HMI und WebUI. Der Legacy-TFT-/Encoder-Stand bleibt als Referenz im selben Repo erhalten.

## Branches und Build-Environments

| Bereich | Branch / Environment | Status |
|---|---|---|
| T4-S3 / LVGL | `feature/t4s3-lvgl-touch` / `lilygo-t4-s3-lvgl` | aktueller Hauptstand |
| Legacy-TFT | `stable/legacy-tft-pre-t4s3` bzw. `feature/legacy-webui-maintenance-settings` / `KaffeewaageLilygoT-DisplayS3_20240212` | finaler klassischer Stand / Referenz |

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
- Stoppuhr
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

### `src/app_state.h`

Gemeinsamer Zustand für WebUI/JSON-Ausgabe. `t4s3_main.cpp` aktualisiert den AppState; `coffee_web.cpp` serialisiert ihn für WebSocket-Clients.

### `src/coffee_web.cpp/.h`

Gemeinsame WebUI- und WebSocket-Schicht für Legacy und T4-S3:

- eingebettete WebUI
- getrennte Asset-Routen `/coffee.css`, `/coffee_core.js`, `/coffee_render.js`, `/coffee_events.js`
- WebSocket-Route `/ws`
- WebSocket-Kommandos
- JSON-State-Erzeugung
- PWA-Manifest-Handler
- Icon-/Favicon-Routen

Das WebUI-Rendering ist browserseitig optimiert: WebSocket-State-Nachrichten werden per `requestAnimationFrame` gebündelt, und DOM-Werte werden nur bei Änderung geschrieben. Das verhindert nach Langlauf die Chrome-Meldungen `message handler took ... ms`.

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

## Waagenlogik und Autodetect

Autodetect, Auto-Tara und Save-ready sind nicht mehr an die aktuell sichtbare HMI-Seite gekoppelt. Dadurch funktioniert die Waagenlogik auch dann, wenn die WebUI auf der Waage-Seite benutzt wird und das lokale HMI gerade Daten, Wartung, WLAN oder System zeigt.

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
