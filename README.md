# Kaffeewaage T4-S3 / Legacy-TFT

Dieses Repo enthält zwei eng verwandte Projektstände der Single-Dose-Kaffeewaage:

| Stand | Branch / Environment | Zweck |
|---|---|---|
| **T4-S3 / LVGL** | `feature/t4s3-lvgl-touch` / `lilygo-t4-s3-lvgl` | aktueller Hauptstand für LilyGO T4-S3 AMOLED Touch mit LVGL-HMI und WebUI |
| **Legacy-TFT** | `feature/legacy-shot-scale` / `KaffeewaageLilygoT-DisplayS3_20240212` | finaler klassischer TFT-/Encoder-Stand mit Gaggiuino-Shot-Waage; Referenz und Rückfallbasis |

Der aktuelle Arbeitsstand dieses ZIPs ist der **T4-S3-/LVGL-Branch**. Die Legacy-Doku und ältere Codepfade bleiben im Repo erhalten, sind aber nicht der primäre Build für neue T4-S3-Arbeiten.

Stabiler Referenz-Tag für diesen Stand: `t4s3-gaggiuino-shot-scale-stable-2026-09-13`.

## Aktueller T4-S3-Stand

Der T4-S3-Stand kombiniert:

- LilyGO T4-S3 AMOLED Touch
- LVGL-HMI in `src/ui_t4s3/`
- echte HX711-Gewichtsmessung über GPIO 42/41
- WebUI mit WebSocket-Live-Status
- WLAN-Seite und Setup-AP `Waagen-Setup`
- OTA-Seite für `firmware.bin` und `spiffs.bin` / `littlefs.bin`
- PWA-/Homescreen-Integration
- NVS/Preferences für UI-, Waagen-, Wartungs- und WLAN-Werte
- WeighMyBru-kompatible BLE-Waage für Gaggiuino
- getrennte Betriebsarten **Single Dose** und **Shot-Waage**
- automatische Shot-Zeitmessung ab erstem **plausiblen** Flüssigkeitszuwachs
- geglättete Flowrate in g/s auf HMI und WebUI
- WebUI-Shotgraph für Gewicht und Flowrate
- letzter Shot ausschließlich im RAM, ohne doppelte dauerhafte Historie

Wichtige zuletzt getestete Punkte:

- Save-Button im HMI verwendet im inaktiven Zustand **nicht** mehr `LV_STATE_DISABLED`, weil der LVGL-Disabled-Theme-State auf dem AMOLED zu hell/lila wirkte. Der Button wird stattdessen optisch dunkelgrau dargestellt und nur über `LV_OBJ_FLAG_CLICKABLE` klickbar/unklickbar gemacht.
- Uptime und Wartungszeiten verwenden getrennte Grammatik: `0 Tage`, `1 Tag`, `2 Tage`; bei Wartung im Kontext `fällig in ...` bzw. `seit ...` wird `Tagen` verwendet.
- Deaktivierte Wartungen erzeugen keine Warnungen und die jeweiligen Zähler für Shots/Mahlgut frieren ein. WebUI zeigt `disabled`, HMI zeigt wegen Platz `-`.
- Autodetect, Auto-Tara und Save-ready laufen unabhängig davon, welche HMI-Seite gerade sichtbar ist. Wizard-/Kalibrier-/Messabläufe pausieren Autodetect weiterhin bewusst.
- Negative Null wird in der HMI-Gewichtsanzeige unterdrückt: gerundete `-0,0 g` wird als `0,0 g` angezeigt.
- Die adaptive HX711-Anzeige reagiert bei großen Änderungen unmittelbar über den FAST-Pfad und bleibt im stabilen Zustand ruhig. Autodetect wartet nicht mehr auf das globale Stable-Flag; ein erkanntes Gefäß wird aktuell `800 ms` bestätigt und dann tariert. Das Abheben eines auto-tarierten Gefäßes bleibt dagegen bewusst an einen stabilen Messwert gekoppelt.
- Der mechanische Endaufbau wurde geprüft. Ein Kabel, das von unten gegen die Wägeplatte drückte, verursachte zuvor einen reproduzierbaren Nachlauf von etwa `0,2–0,3 g` nach Be- und Entlastung. Nach Freilegen des Kabels bleibt der Nullpunkt über wiederholte Auflege-/Abhebezyklen stabil.
- Die HX711-Diagnose kann ohne USB über die Home-WebUI aufgezeichnet werden. Ein PSRAM-Ringpuffer speichert 10-Hz-Zwischenwerte und Ereignisse; CSV-Export und ein manueller `Shot abbrechen`-Befehl stehen zur Verfügung.
- WebUI-WebSocket-Rendering ist per `requestAnimationFrame` gebündelt und schreibt DOM-Werte nur bei Änderung. Während `armed/running` wird zusätzlich alle `200 ms` ein kompaktes `shot_telemetry`-Paket gesendet; der große Gesamtzustand läuft dann mit `1000 ms`, außerhalb eines Shots mit `500 ms`.
- BLE startet fünf Sekunden nach dem Boot. Die Verbindung bleibt unabhängig vom aktiven Modus bestehen; Maschinenkommandos werden nur auf der Shot-Seite ausgeführt.
- Gaggiuino sendet im getesteten WeighMyBru-Betrieb zuverlässig `TARE`, jedoch keine Timerkommandos. Die Shot-Erkennung kombiniert deshalb Gewichtsschwellen mit einer Trend-Plausibilisierung über ca. `1,2 s`, um Pumpenvibrationen von echtem Flüssigkeitszuwachs zu unterscheiden.
- Frühe Fehlstarts werden zusätzlich abgefangen: ein passendes Gaggiuino-Tara kann einen sehr frühen, leichten Fehlstart zurücksetzen; ein Watchdog verwirft eine Session, die in den ersten Sekunden ohne Flow wieder nahe Null fällt.
- Der letzte Shot wird mit maximal 1.200 Samples bei 10 Hz im RAM gehalten. Ein neuer Shot überschreibt den alten erst beim tatsächlichen Start.
- Das lokale HMI ist Master für die aktive Haupt- und Einstellungsseite. Neu verbundene WebUI-Geräte übernehmen diesen Zustand.
- WebUI-Assets verwenden eine zentrale Versionskennung und werden im Produktivbetrieb langfristig mit `immutable` gecacht.

## Build

Für den aktuellen T4-S3-Stand immer gezielt das LVGL-Environment bauen:

```powershell
pio run -e lilygo-t4-s3-lvgl
```

Für den stabilen Referenzstand werden die getesteten Kernabhängigkeiten in `platformio.ini` exakt gepinnt. Relevant sind insbesondere:

```text
HX711_ADC          1.2.12
ESPAsyncWebServer  3.11.0
AsyncTCP           3.4.10
ArduinoJson        7.4.3
```

Damit zieht ein späterer Clean-Build nicht unbeabsichtigt eine neuere AsyncWebServer-/AsyncTCP-Kombination ein.

Upload per USB:

```powershell
pio run -e lilygo-t4-s3-lvgl -t upload
```

Hinweis: Ein nacktes `pio run` kann auch das alte Legacy-Environment mitbauen. Dieses kann wegen T4-S3-/LVGL-Dateien und anderer Bibliotheken fehlschlagen, obwohl der relevante T4-S3-Build erfolgreich ist. Für T4-S3-Arbeiten deshalb immer `-e lilygo-t4-s3-lvgl` verwenden.

Legacy-Build nur bei Arbeit am Legacy-Branch:

```powershell
pio run -e KaffeewaageLilygoT-DisplayS3_20240212
```

## Sicheres Analyse-ZIP

Für Repo-Analysen immer das Export-Tool verwenden:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\export_repo_zip.ps1
```

Das erzeugte ZIP liegt automatisch im Downloads-Ordner und schließt Secrets aus.

## OTA-Update

Die T4-S3-Waage hat eine eigene Update-Seite:

```text
http://<ip-der-waage>/update
```

Erlaubte Dateinamen:

| Upload | Datei |
|---|---|
| Firmware | `firmware.bin` |
| Dateisystem | `spiffs.bin` oder `littlefs.bin` |

Falsche Dateien werden client- und serverseitig abgelehnt. Nach erfolgreichem Upload startet der ESP nicht automatisch neu; der Neustart erfolgt per Button auf der Update-Seite.

## Dateisystem bauen

Bei OTA-Nutzung nicht `uploadfs` verwenden. Stattdessen nur das Dateisystem-Image bauen:

```powershell
pio run -e lilygo-t4-s3-lvgl -t buildfs
```

Danach die erzeugte Datei aus `.pio\build\lilygo-t4-s3-lvgl\` über die OTA-Dateisystem-Upload-Seite hochladen.

## WebUI

Die Haupt-WebUI wird weiterhin aus `src/coffee_web.cpp` ausgeliefert, ist aber intern in mehrere Assets aufgeteilt:

| Route | Bedeutung |
|---|---|
| `/` | HTML-Grundgerüst |
| `/coffee.css` | Stylesheet |
| `/coffee_core.js` | Basisfunktionen, Navigation, Overlays |
| `/coffee_render.js` | State-Rendering, Wartung, Daten |
| `/coffee_events.js` | WebSocket und Events |
| `/ws` | WebSocket für Live-State und Kommandos |
| `/update` | OTA-Update-Seite |
| `/manifest.json` | PWA-Manifest |
| `/icon-192.png` | 192x192 Homescreen-Icon aus Dateisystem |
| `/icon-512.png` | 512x512 Homescreen-Icon aus Dateisystem |
| `/favicon.ico` | Browser-Favicon aus Dateisystem |

## Dokumentation

Details stehen im Ordner `docs/`:

- `docs/architecture.md`
- `docs/hmi-ui-concept.md`
- `docs/hx711_architektur_und_filterlogik.md`
- `docs/t4s3-hardware-pins.md`
- `docs/t4s3-persistent-settings.md`
- `docs/webui-pwa-ota.md`
- `docs/wifi-provisioning.md`
- `docs/ota_filename_validation.md`
- `docs/storage.md`
- `docs/test-plan.md`
- `docs/ble-shot-scale.md`
- `docs/weight-diagnostics.md`

## Beobachtungspunkt

Ein einmaliger TFT-/HMI-Grafikfehler mit invertierten Farben bzw. hellem Hintergrund wurde früher beobachtet, war aber bisher nicht reproduzierbar. Zusätzlich trat einmal nach einem Shot beim Wechsel zurück auf Home ein vollständiger HMI-/WebUI-Freeze auf; auch dieser ließ sich in wiederholten Seitenwechsel- und Diagnose-Polling-Tests nicht reproduzieren. Bei erneutem Auftreten Zeitpunkt, Bedienkontext und wenn möglich Diagnosezustand notieren.
