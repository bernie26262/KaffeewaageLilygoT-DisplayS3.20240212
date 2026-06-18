# Kaffeewaage ESP32 Single Dose

ESP32-Projekt fuer eine Single-Dose-Kaffeewaage auf Basis eines LilyGO T-Display S3.

Das Projekt kombiniert:

- HX711-Waegemessung
- TFT-Bedienung am Geraet
- Rotary-Encoder-/Taster-Bedienung
- WLAN-WebUI mit WebSocket-Live-Status
- OTA-Update fuer Firmware und SPIFFS-Dateisystem
- persistente Einstellungen ueber NVS/Preferences
- PWA-/Homescreen-Integration fuer Smartphone/Tablet
- WeighMyBru-kompatible BLE-Waage, mit Gaggiuino praktisch getestet
- automatische Shot-Erkennung mit Timer, Gewicht, Flow und Live-Graph
- HMI-orientiertes Bedienkonzept als Basis fuer eine spaetere Touch-Display-Version

## Aktueller Architekturstand

Die groessten Projektbereiche sind inzwischen aus `main.cpp` herausgeloest:

| Bereich | Dateien | Zweck |
|---|---|---|
| App-State | `src/app_state.h` | Gemeinsamer Zustand fuer WebUI/JSON-Ausgabe |
| WebUI/WebSocket | `src/coffee_web.h`, `src/coffee_web.cpp` | Eingebettete WebUI, WebSocket-Kommandos, PWA-Manifest/Icon-Routen |
| OTA | `src/coffee_ota.h`, `src/coffee_ota.cpp` | `/update`-Seite fuer Firmware- und SPIFFS-Upload |
| Storage | `src/coffee_storage.h`, `src/coffee_storage.cpp` | NVS/Preferences lesen/schreiben |
| BLE-Waage | `src/ble_scale.h`, `src/ble_scale.cpp` | WeighMyBru-GATT-Service, Gewichtsmeldungen, Maschinenkommandos und RAM-Diagnoselog |
| Shot-Session | `src/shot_session.h`, `src/shot_session.cpp` | automatische Shot-Erkennung, Zeit, Messpunkte und geglaettete Flowrate |
| Hardware/UI-Hauptlogik | `src/main.cpp` | Waage, TFT, Taster/Encoder, Modus- und Ablaufsteuerung |
| Pins | `src/pin_config.h` | Pinbelegung |
| WebUI-Dateien im SPIFFS | `data/` | PWA-/Homescreen-Icons |

## Build

Standard-Environment:

```powershell
pio run -e KaffeewaageLilygoT-DisplayS3_20240212
```

## OTA-Update

Die Kaffeewaage hat eine eigene Update-Seite:

```text
http://<ip-der-kaffeewaage>/update
```

Dort koennen getrennt hochgeladen werden:

- Firmware-Binary
- SPIFFS-Dateisystem-Binary

Nach erfolgreichem Upload startet der ESP nicht automatisch neu. Der Neustart erfolgt bewusst per Button auf der Update-Seite.

## SPIFFS-Dateisystem bauen

Bei OTA-Nutzung nicht `uploadfs` verwenden. Stattdessen nur das Dateisystem-Image bauen:

```powershell
pio run -e KaffeewaageLilygoT-DisplayS3_20240212 -t buildfs
```

Danach die erzeugte Datei aus `.pio\build\KaffeewaageLilygoT-DisplayS3_20240212\` ueber die OTA-Dateisystem-Upload-Seite hochladen.

Je nach PlatformIO-/Partition-Konfiguration heisst die Datei z.B.:

```text
spiffs.bin
```

## WebUI

Die Haupt-WebUI ist aktuell noch als eingebetteter HTML/CSS/JS-String in `src/coffee_web.cpp` umgesetzt. Die PWA-Icons liegen dagegen bereits im SPIFFS-Dateisystem unter `data/`.

Wichtige Routen:

| Route | Bedeutung |
|---|---|
| `/` | Haupt-WebUI |
| `/ws` | WebSocket fuer Live-State und Kommandos |
| `/api/shot/samples` | inkrementeller RAM-Verlauf des laufenden oder letzten Shots |
| `/update` | OTA-Update-Seite |
| `/manifest.json` | PWA-Manifest aus Firmware |
| `/icon-192.png` | 192x192 Homescreen-Icon aus SPIFFS |
| `/icon-512.png` | 512x512 Homescreen-Icon aus SPIFFS |
| `/favicon.ico` | Browser-Favicon aus SPIFFS |

## Dokumentation

Details stehen im Ordner `docs/`:

- `docs/architecture.md`
- `docs/ble-shot-scale.md`
- `docs/hmi-ui-concept.md`
- `docs/webui-pwa-ota.md`
- `docs/wifi-provisioning.md`
- `docs/ota_filename_validation.md`
- `docs/storage.md`
- `docs/test-plan.md`

## Hinweise zum aktuellen Cleanup-Stand

Bereits erledigt:

- PWA-/Homescreen-Icons integriert
- OTA-Komponente ausgelagert
- OTA-Dateinamen-Schutz fuer Firmware- und SPIFFS-Uploads integriert
- Storage-Komponente eingefuehrt
- alte direkte Preferences-Kommentarreste aus `main.cpp` entfernt
- alter Kaninchenheizung-Web-Placeholder-Processor entfernt
- alte auskommentierte Debug-/Testbloecke entfernt

Beobachtungspunkt:

- Ein einmaliger TFT-/HMI-Grafikfehler mit invertierten Farben bzw. hellem Hintergrund wurde beobachtet, war aber bisher nicht reproduzierbar.
