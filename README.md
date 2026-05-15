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

## Aktueller Architekturstand

Die groessten Projektbereiche sind inzwischen aus `main.cpp` herausgeloest:

| Bereich | Dateien | Zweck |
|---|---|---|
| App-State | `src/app_state.h` | Gemeinsamer Zustand fuer WebUI/JSON-Ausgabe |
| WebUI/WebSocket | `src/coffee_web.h`, `src/coffee_web.cpp` | Eingebettete WebUI, WebSocket-Kommandos, PWA-Manifest/Icon-Routen |
| OTA | `src/coffee_ota.h`, `src/coffee_ota.cpp` | `/update`-Seite fuer Firmware- und SPIFFS-Upload |
| Storage | `src/coffee_storage.h`, `src/coffee_storage.cpp` | NVS/Preferences lesen/schreiben |
| Hardware/UI-Hauptlogik | `src/main.cpp` | Waage, TFT, Taster/Encoder, Ablaufsteuerung |
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
| `/update` | OTA-Update-Seite |
| `/manifest.json` | PWA-Manifest aus Firmware |
| `/icon-192.png` | 192x192 Homescreen-Icon aus SPIFFS |
| `/icon-512.png` | 512x512 Homescreen-Icon aus SPIFFS |
| `/favicon.ico` | Browser-Favicon aus SPIFFS |

## Dokumentation

Details stehen im Ordner `docs/`:

- `docs/architecture.md`
- `docs/webui-pwa-ota.md`
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
