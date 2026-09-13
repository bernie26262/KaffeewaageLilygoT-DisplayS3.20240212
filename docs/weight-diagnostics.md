# Weight-Diagnose der T4-S3-Waage

## Zweck

Die fertig montierte T4-S3-Waage hat im Housing keinen bequem zugänglichen USB-Port. Für die Analyse von Responsiveness, Drift, Pumpenvibration und Shot-Start existiert deshalb ein USB-unabhängiger Diagnose-Recorder.

Implementierung:

```text
src/t4s3_weight_diag.cpp
src/t4s3_weight_diag.h
```

Bedienung erfolgt in der WebUI unter `Einstellungen -> System / OTA -> Gewichtsdiagnose`.

## Bedienung

Verfügbare Aktionen:

- `Start`: bisherigen Puffer leeren und neue Aufnahme beginnen
- `Stop`: Aufnahme anhalten
- `Löschen`: Puffer leeren
- `CSV`: komplette Aufnahme herunterladen
- `Shot abbrechen`: laufende/armed Shot-Session manuell auflösen

Die Aufnahme läuft im ESP weiter, auch wenn in der WebUI auf die Shot-Seite oder eine andere Seite gewechselt wird.

## Puffer

- Sampleintervall: `100 ms` / 10 Hz
- bevorzugter PSRAM-Puffer: 9.000 Samples / ca. 15 Minuten
- Fallback ohne PSRAM-Allokation: 1.200 Samples / ca. 2 Minuten
- Ringpuffer: bei Überlauf werden die ältesten Samples ersetzt

Die Liveansicht der WebUI ist nur eine Darstellung. Sie bestimmt nicht den Aufnahmetakt. Das Live-Polling läuft nur, solange `System / OTA -> Gewichtsdiagnose` sichtbar und aufgeklappt ist; die Aufnahme im ESP läuft unabhängig davon weiter.

## CSV-Felder

Die CSV enthält unter anderem:

| Feld | Bedeutung |
|---|---|
| `timestamp_ms` / elapsed | Zeitbezug |
| `library_raw` | unmittelbarer `HX711_ADC::getData()`-Wert |
| `median_raw` | zusätzlicher Median-Vorfilter |
| `library_g` | Library-Wert in Gramm |
| `median_g` | Median-Wert in Gramm |
| `fast_g` | schneller Single-Dose-Pfad |
| `stable_g` | ruhiger Stable-Pfad |
| `display_g` | tatsächlich verwendeter Anzeigeweg |
| `shot_g` | eigener Shot-Pfad |
| `flow_g_s` | aktuelle Shot-Flowrate |
| `moving` | Bewegung erkannt |
| `stable` | Stable-Zustand |
| `shot_state` | Zustand der Shot-Session |
| `events` | Ereignis-Markierungen |

## Ereignisse

Aktuell werden unter anderem markiert:

```text
REC_START
AUTO_TARE
TARE_BEGIN
TARE_DONE
BLE_TARE
BLE_TIMER_START
BLE_TIMER_STOP
BLE_TIMER_RESET
MOVEMENT
STABLE
SHOT_ARM
SHOT_START
SHOT_STOP
SHOT_ABORT
SHOT_DISARM
SHOT_FALSE_START
```

Die BLE-Timer-Ereignisse sind bewusst enthalten, obwohl die aktuell getestete Gaggiuino-WeighMyBru-Integration bislang nur `TARE` und keine Timerkommandos gesendet hat.

## Bewährte Testabläufe

### Responsiveness / Auto-Tara

1. Waage mehrere Minuten laufen lassen.
2. Diagnose starten.
3. 10 s leer.
4. bekanntes Gefäß auflegen.
5. Auto-Tara abwarten.
6. 30 s unverändert stehen lassen.
7. Gefäß abnehmen.
8. weitere 30 s warten.
9. Stop und CSV.

Damit lassen sich Lastsprung, Auto-Tara und möglicher Nachlauf nach Be- und Entlastung direkt vergleichen.

### Pumpenvibration ohne echten Gewichtszuwachs

1. Gefäß mehrere Minuten auf der Waage stehen lassen.
2. manuell tarieren.
3. Diagnose starten.
4. kurze Ruhephase.
5. Vibrationspumpe laufen lassen, Wasser aber nicht in das Gefäß auf der Waage leiten.
6. Pumpe aus und kurze Ruhephase.
7. Stop und CSV.

Dieser Test zeigte, dass reine Pumpenvibration im SHOT-Pfad zeitweise scheinbare positive Werte über der früheren `0,45-g`-Startschwelle erzeugen kann. Daraus entstand die heutige Trend-Plausibilisierung der Shot-Startlogik.

### Echter Shot

1. Unter `Einstellungen -> System / OTA -> Gewichtsdiagnose` die Aufnahme starten.
2. zur Shot-Seite wechseln.
3. Shot normal durchführen.
4. nach Shot-Ende und abschließendem Gaggiuino-Tara noch einige Sekunden warten.
5. zurück zu `Einstellungen -> System / OTA`.
6. Diagnose stoppen und CSV herunterladen.

Die Aufnahme bleibt während der Seitenwechsel aktiv.

## Erkenntnisse aus September 2026

- Der starke scheinbare Drift von etwa `0,2–0,3 g` war überwiegend mechanisch: ein Kabel drückte gegen die Wägeplatte.
- Nach Freilegen des Kabels blieb der Nullpunkt über mehrere Auflege-/Abhebezyklen stabil.
- Die Vibrationspumpe kann ohne Flüssigkeitszuwachs mehr als `0,45 g` scheinbares Shot-Gewicht erzeugen.
- Eine reine Schwellwert-/Zeit-Startlogik ist deshalb nicht robust genug.
- Die aktuelle Trendprüfung verhindert die getesteten Pumpen-Fehlstarts bei weiterhin funktionierenden realen Shots.
- Ein einmaliger kompletter HMI-/WebUI-Freeze nach einem Shot ließ sich in gezielten Seitenwechsel-/Polling-Tests nicht reproduzieren und bleibt Beobachtungspunkt.
