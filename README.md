# Kaffeewaage T4-S3 / Legacy-TFT

Dieses Repo enthält zwei eng verwandte Projektstände der Single-Dose-Kaffeewaage:

| Stand | Branch / Environment | Zweck |
|---|---|---|
| **T4-S3 / LVGL** | `feature/t4s3-lvgl-touch` / `lilygo-t4-s3-lvgl` | aktueller Hauptstand für LilyGO T4-S3 AMOLED Touch mit LVGL-HMI und WebUI |
| **Legacy-TFT** | `feature/legacy-shot-scale` / `KaffeewaageLilygoT-DisplayS3_20240212` | finaler klassischer TFT-/Encoder-Stand mit Gaggiuino-Shot-Waage; Referenz und Rückfallbasis |

Der aktuelle Arbeitsstand dieses ZIPs ist der **T4-S3-/LVGL-Branch**. Die Legacy-Doku und ältere Codepfade bleiben im Repo erhalten, sind aber nicht der primäre Build für neue T4-S3-Arbeiten.

Letzter vor dieser Portierung gesetzter stabiler Referenz-Tag: `t4s3-gaggiuino-shot-scale-stable-2026-09-13`. Die WLAN-/OTA-Portierung ist bis Commit `5307cf2` (`feature/t4s3-lvgl-touch`) implementiert und hardwaregetestet; der neue stabile Tag wird erst nach Abschlussprüfung und Dokumentation gesetzt.

## Aktueller T4-S3-Stand

Der T4-S3-Stand kombiniert:

- LilyGO T4-S3 AMOLED Touch
- LVGL-HMI in `src/ui_t4s3/`
- echte HX711-Gewichtsmessung über GPIO 42/41
- WebUI mit WebSocket-Live-Status
- WLAN-Seite mit zwei gespeicherten Profilen, Preferred-Auswahl und automatischem Failover
- manueller Setup-AP (`AP+STA`) und automatischer Recovery-AP (`AP-only`) als `Waagen-Setup`
- OTA ausschließlich für `firmware.bin`, mit BLE-Abschaltung und Neustart-Sperre nach Erfolg
- PWA-/Homescreen-Icons direkt in der Firmware statt SPIFFS
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
- Die HX711-Diagnose kann ohne USB unter `Einstellungen -> System / OTA -> Gewichtsdiagnose` aufgezeichnet werden. Ein PSRAM-Ringpuffer speichert 10-Hz-Zwischenwerte und Ereignisse; CSV-Export und ein manueller `Shot abbrechen`-Befehl stehen zur Verfügung.
- WebUI-WebSocket-Rendering ist per `requestAnimationFrame` gebündelt und schreibt DOM-Werte nur bei Änderung. Während `armed/running` wird zusätzlich alle `200 ms` ein kompaktes `shot_telemetry`-Paket gesendet; der große Gesamtzustand läuft dann mit `1000 ms`, außerhalb eines Shots mit `500 ms`.
- BLE startet fünf Sekunden nach dem Boot. Die Verbindung bleibt unabhängig vom aktiven Modus bestehen; Maschinenkommandos werden nur auf der Shot-Seite ausgeführt.
- Gaggiuino sendet im getesteten WeighMyBru-Betrieb zuverlässig `TARE`, jedoch keine Timerkommandos. Die Shot-Erkennung kombiniert deshalb Gewichtsschwellen mit einer Trend-Plausibilisierung über ca. `1,2 s`, um Pumpenvibrationen von echtem Flüssigkeitszuwachs zu unterscheiden.
- Frühe Fehlstarts werden zusätzlich abgefangen: ein passendes Gaggiuino-Tara kann einen sehr frühen, leichten Fehlstart zurücksetzen; ein Watchdog verwirft eine Session, die in den ersten Sekunden ohne Flow wieder nahe Null fällt.
- Der letzte Shot wird mit maximal 1.200 Samples bei 10 Hz im RAM gehalten. Ein neuer Shot überschreibt den alten erst beim tatsächlichen Start.
- Das lokale HMI ist Master für die aktive Haupt- und Einstellungsseite. Neu verbundene WebUI-Geräte übernehmen diesen Zustand.
- WebUI-Assets verwenden eine zentrale Versionskennung und werden im Produktivbetrieb langfristig mit `immutable` gecacht.

Die Portierung der Legacy-WLAN-/OTA-Verbesserungen wurde bis `5307cf2` auf der T4-S3 bestätigt: zwei WLAN-Profile, Failover und AP-only-Recovery einschließlich WebUI-Retry; BLE aus während gültigem OTA; PWA-Dateien ohne SPIFFS; neue Firmware-OTA-Seite. Nach der Portierung wurde auch die Shot-WebUI auf Hardware getestet; ihre Reaktivität ist unverändert ausreichend. Ein absichtlich abgebrochener OTA-Upload wurde noch nicht auf Hardware geprüft.

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

## OTA-Update (nur Firmware)

Die T4-S3 stellt die Update-Seite unter `http://<ip-der-waage>/update` bereit. Gebaut wird mit:

```powershell
pio run -e lilygo-t4-s3-lvgl
```

Die Datei für den Upload liegt unter `.pio\build\lilygo-t4-s3-lvgl\firmware.bin`. Nur **`firmware.bin`** ist zulässig; Name und Dateiauswahl werden im Browser und auf dem ESP geprüft. Die Seite zeigt Fortschritt und eine Warnung, sperrt während des Uploads die Navigation und schaltet BLE beim Start eines gültigen Firmware-Uploads bis zum Neustart ab.

Nach erfolgreichem Upload ist ein weiterer Upload auch nach Neuladen der Seite gesperrt; nur ein bewusst angeforderter ESP32-Neustart aktiviert die neue Firmware. Nach der Neustartanforderung erscheint „Zurück zur WebUI“ wieder. Abgebrochene oder fehlgeschlagene Uploads können vollständig bei 0 % neu begonnen werden (kein Byte-Resume); ein absichtlicher Hardware-Abbruchtest steht noch aus.

**Kein Dateisystem-Image nötig:** `buildfs`, `uploadfs`, `spiffs.bin`, `littlefs.bin` und `/update/filesystem` gehören nicht mehr zum aktuellen T4-S3-Updateprozess. Die vorhandene Partitionstabelle wurde dabei nicht verändert.

Details: `docs/webui-pwa-ota.md`, `docs/ota_filename_validation.md`.

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
| `/update` | OTA-Update-Seite (nur Firmware) |
| `/update/status` | OTA-Neustartstatus als JSON |
| `/manifest.json` | PWA-Manifest |
| `/icon-192.png` | 192x192 Homescreen-Icon aus eingebettetem Flash |
| `/icon-512.png` | 512x512 Homescreen-Icon aus eingebettetem Flash |
| `/favicon.ico` | Browser-Favicon aus eingebettetem Flash |

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

## Offene Punkte / Beobachtung

- Ein absichtlich unterbrochener Firmware-OTA-Upload ist bislang nur durch Code-/Simulationstests, nicht durch Hardware-Abbruch validiert.
- Die vollständige normale WebUI auf `192.168.4.1` ist bewusst zurückgestellt; dort gibt es die separate Setup-/Recovery-Seite.
- Nach der Portierung zeigte der Shotwaagen-Test keine relevante Verlangsamung der WebUI.

## Historische Beobachtungen

Ein einmaliger TFT-/HMI-Grafikfehler mit invertierten Farben bzw. hellem Hintergrund wurde früher beobachtet, war aber bisher nicht reproduzierbar. Zusätzlich trat einmal nach einem Shot beim Wechsel zurück auf Home ein vollständiger HMI-/WebUI-Freeze auf; auch dieser ließ sich in wiederholten Seitenwechsel- und Diagnose-Polling-Tests nicht reproduzieren. Bei erneutem Auftreten Zeitpunkt, Bedienkontext und wenn möglich Diagnosezustand notieren.
