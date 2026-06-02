# Kaffeewaage ESP32 Single Dose

ESP32-Projekt fuer eine Single-Dose-Kaffeewaage auf Basis eines LilyGO T-Display S3.

Dieser Branch dokumentiert den Legacy-TFT-Stand. Die neue T4-S3-/LVGL-Variante liegt im separaten Branch `feature/t4s3-lvgl-touch` und sollte nicht blind mit diesem Stand vermischt werden.

Das Projekt kombiniert:

- HX711-Waegemessung
- TFT-Bedienung am Geraet
- Rotary-Encoder-/Taster-Bedienung
- WLAN-WebUI mit WebSocket-Live-Status
- OTA-Update fuer Firmware und SPIFFS-Dateisystem
- persistente Einstellungen ueber NVS/Preferences
- PWA-/Homescreen-Integration fuer Smartphone/Tablet
- HMI-orientiertes Bedienkonzept als historische Basis fuer die separate T4-S3-/LVGL-Variante

## Aktueller Architekturstand

Die groessten Projektbereiche sind inzwischen aus `main.cpp` herausgeloest:

| Bereich | Dateien | Zweck |
|---|---|---|
| App-State | `src/app_state.h` | Gemeinsamer Zustand fuer WebUI/JSON-Ausgabe |
| WebUI/WebSocket | `src/coffee_web.h`, `src/coffee_web.cpp` | Eingebettete WebUI, WebSocket-Kommandos, PWA-Manifest/Icon-Routen, gebuendeltes State-Rendering |
| OTA | `src/coffee_ota.h`, `src/coffee_ota.cpp` | `/update`-Seite fuer Firmware- und SPIFFS-Upload |
| Storage | `src/coffee_storage.h`, `src/coffee_storage.cpp` | NVS/Preferences lesen/schreiben |
| Hardware/UI-Hauptlogik | `src/main.cpp` | Waage, Legacy-TFT, Taster/Encoder, Ablaufsteuerung |
| Pins | `src/pin_config.h` | Pinbelegung |
| WebUI-Dateien im SPIFFS | `data/` | PWA-/Homescreen-Icons |

## Build

Legacy-Branch:

```text
feature/legacy-webui-maintenance-settings
stable/legacy-tft-pre-t4s3
```

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

Die Haupt-WebUI ist weiterhin in `src/coffee_web.cpp` eingebettet, aber in getrennte Firmware-Routen fuer HTML, CSS und JavaScript aufgeteilt. Der WebSocket-State wird im Browser gebuendelt gerendert, damit lang geoeffnete WebUI-Sessions keine `message handler took ... ms`-Violations erzeugen. Die PWA-Icons liegen im SPIFFS-Dateisystem unter `data/`.

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
- Wartungszähler frieren bei deaktivierter Wartung ein und laufen nach Reaktivierung weiter
- Autodetect/Auto-Tara laufen unabhaengig von der aktuell sichtbaren Legacy-TFT-Seite
- WebUI-Rendering fuer lange Laufzeit per requestAnimationFrame und DOM-Deduplizierung entlastet

Beobachtungspunkt:

- Ein einmaliger TFT-/HMI-Grafikfehler mit invertierten Farben bzw. hellem Hintergrund wurde beobachtet, war aber bisher nicht reproduzierbar.
