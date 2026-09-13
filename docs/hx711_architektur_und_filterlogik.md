# HX711 Architektur und Filterlogik

## Ziel

Die T4-S3-Kaffeewaage verwendet den HX711 bei 80 Hz und soll gleichzeitig schnell auf Laständerungen reagieren, im Stillstand ruhig bleiben und während eines Espresso-Shots eine belastbare Gewichtskurve liefern.

Aktueller getesteter Stand: September 2026.

Wichtige Anforderungen:

- direkte Reaktion beim Auflegen / Entfernen
- ruhige Endanzeige
- schnelle und reproduzierbare Auto-Tara
- zuverlässige Gefäßerkennung
- eigener ruhiger Shot-Pfad
- Diagnose ohne USB-Zugang
- robuste Funktion im fertig montierten Housing

---

## Hardware

### Aktueller Aufbau

ESP32 T4-S3 AMOLED:

- HX711 DT -> GPIO 42
- HX711 SCK -> GPIO 41
- HX711-Versorgung über 3,3 V
- HX711 auf 80 Hz
- fertiger mechanischer Aufbau im Housing

Die Firmware verwendet `HX711_ADC 1.2.12`. Der Bibliotheks-Kalibrierfaktor bleibt auf `1.0`, sodass `getData()` als bereits bibliotheksseitig geglätteter, raw-artiger Eingang in die eigene T4-S3-Pipeline eingeht. Die Umrechnung in Gramm erfolgt anschließend über den gespeicherten Faktor `raw/g`.

### Mechanik ist Teil der Messkette

Bei den Vergleichstests zeigte sich ein wichtiger mechanischer Fehler: Ein Kabel vom Biegebalken zum HX711 drückte von unten gegen die Wägeplatte. Dadurch entstand nach Belastung ein positiver und nach Entlastung ein negativer Nachlauf von ungefähr `0,2–0,3 g`.

Nach Freilegen des Kabels verschwand dieses spiegelbildliche Creep-/Hysterese-Muster praktisch vollständig. Wiederholte Auflege-/Abhebezyklen blieben anschließend um Null stabil.

Daraus folgt als feste Integrationsregel:

- Wägeplatte muss frei beweglich sein
- keine Kabel dürfen Wägeplatte oder beweglichen Teil des Biegebalkens berühren
- Kabel mit ausreichender Schlaufe/Zugentlastung führen
- bei unerklärlichem Drift zuerst mechanische Nebenkräfte prüfen, bevor Softwarefilter verändert werden

---

## Warum 80 Hz

10 Hz war im frühen Test sehr ruhig, aber für schnelle Gewichtsänderungen und Shot-Anzeige zu träge.

80 Hz liefert:

- schnelle Sprungantwort
- genügend Dynamik für Espresso
- mehr Rauschen als 10 Hz

Die höhere Rohdynamik wird deshalb durch mehrere getrennte Signalpfade aufbereitet.

---

## Aktuelle Messarchitektur

### 1. HX711_ADC / Library-Wert

`HX711_ADC` übernimmt die unmittelbare HX711-Abfrage und liefert über `getData()` einen bereits geglätteten Bibliothekswert. Dieser Wert wird in der Weight-Diagnose als `library_raw` beziehungsweise nach Umrechnung als `library_g` protokolliert.

### 2. Median-Vorfilter

Auf den Bibliothekswert folgt ein zusätzlicher 5er-Median. Er unterdrückt einzelne Ausreißer, kann aber mehrsampleige Störungen nicht vollständig entfernen.

Diagnosefelder:

- `median_raw`
- `median_g`

### 3. FAST PATH

Schneller adaptiver EMA für Single Dose und Lastsprünge.

Eigenschaften:

- Basis-Alpha `0.35`
- mittlere Änderungen: `0.55`
- große Änderungen: `0.80`
- reagiert beim Auflegen/Abheben sichtbar direkt

Während Bewegung **und Settling** folgt die Anzeige diesem FAST-Pfad. Das frühere Zurückblenden zum langsameren STABLE-Pfad während der Settling-Phase wurde entfernt, weil es nach großen Sprüngen sichtbar hinterherlief.

### 4. STABLE PATH

Langsamer, ruhiger Referenzpfad.

Eigenschaften:

- Basis-Alpha `0.05`
- mittlere Änderungen: `0.12`
- große Änderungen: `0.18`
- Stable-Delay aktuell `700 ms`

Der STABLE-Pfad bleibt wichtig für Ruhe-/Stabilitätsinformation. Die Gefäßerkennung selbst wartet jedoch nicht mehr auf das globale Stable-Flag.

### 5. DISPLAY PATH

Adaptive Single-Dose-Anzeige:

- `moving` oder noch nicht `stable`: direkt FAST
- `stable`: nur langsames Nachführen, wenn die Abweichung die Display-Deadband überschreitet
- `kDisplayStableDeadbandGrams = 0.08 g`
- gerundete negative Null wird als `0,0 g` dargestellt

Das Ergebnis ist eine sehr schnelle Sprungantwort bei gleichzeitig ruhiger Endanzeige.

### 6. SHOT PATH

Eigener ruhiger EMA für den Espresso-Shot:

- Basis-Alpha `0.10`
- Catch-up-Alpha `0.24`
- Catch-up ab `4,0 g` Abweichung

Der Shot-Pfad wird verwendet für:

- Shot-Gewicht
- Gaggiuino-Gewichtsübertragung im Shot-Modus
- Shot-Start/-Stop
- Sample-Puffer
- Flowrate

Single Dose und Shot bleiben dadurch getrennt optimierbar.

---

## Autodetect und Auto-Tara

Die aktuelle Gefäßerkennung arbeitet auf der sichtbaren Single-Dose-Anzeige und ist nicht mehr an das globale Stable-Flag gekoppelt.

Ablauf:

1. Gewicht springt über FAST unmittelbar auf das Gefäßgewicht.
2. Ein gespeichertes Gefäß innerhalb der Toleranz wird erkannt.
3. Der Treffer muss `800 ms` bestehen.
4. Auto-Tara wird ausgeführt.
5. Beim späteren Abheben wird erst dann leer tariert, wenn mindestens `30 g` negative Änderung **und** `stable` vorliegen.

Der frühere Ablauf `stable abwarten + zusätzlich 800 ms` fühlte sich deutlich träger an. Die aktuelle Variante erreicht im fertigen Aufbau typischerweise eine Tara ungefähr 1,4–1,6 s nach dem erkannten Lastsprung und blieb nach Behebung des Kabelkontakts stabil um Null.

---

## Shot-Start unter Pumpenvibration

Reine Vibrationspumpen-Tests zeigten, dass der SHOT-Pfad ohne realen Flüssigkeitszuwachs zeitweise scheinbare positive Werte von mehr als `0,45 g` erzeugen kann. Eine reine Schwellwertlogik ist deshalb nicht ausreichend.

Aktuelle Startlogik:

- Arm-Settling `500 ms`
- Kandidat ab `0,25 g`
- Bestätigung ab `0,45 g` und mindestens `250 ms`
- zusätzlich Trendfenster ca. `1,2 s`
- Mindestspanne `0,9 s`
- Mindest-Nettozuwachs `0,15 g`
- Mindeststeigung `0,08 g/s`
- maximaler RMSE zum linearen Trend `0,05 g`

Die Parameter wurden gegen mehrere reine Pumpenläufe und reale Espresso-Shots geprüft. Pumpenvibration allein führte nicht mehr zum Shot-Start.

Als zusätzliche Sicherung existiert ein False-Start-Watchdog in den ersten 6 s sowie Recovery durch ein passendes Gaggiuino-Tara.

Details: `docs/ble-shot-scale.md`.

---

## Flowrate

Die Flowrate ist die Steigung der Gewichtskurve in g/s. Sie wird nicht aus zwei Einzelpunkten berechnet, sondern geglättet:

- Shot-Samples: 10 Hz
- Regressionsfenster: 2,5 s
- Mindestspanne: 1,2 s
- mindestens 10 Samples
- lineare Regression Gewicht gegen Zeit
- EMA Alpha `0.15`
- Deadband `0.05 g/s`
- Begrenzung auf `20 g/s`

Die Flowrate verändert weder Rohmessung noch Single-Dose-Stabilitätslogik.

---

## Weight-Diagnose

Für den eingebauten Zustand ohne USB existiert `src/t4s3_weight_diag.cpp/.h`.

Die Home-WebUI kann die Aufzeichnung starten/stoppen, löschen und als CSV herunterladen. Aufgezeichnet werden unter anderem:

- `library_g`
- `median_g`
- `fast_g`
- `stable_g`
- `display_g`
- `shot_g`
- Flow
- Moving/Stable
- Shot-State
- Tara-/BLE-/Shot-Ereignisse

Der Logger arbeitet mit 10 Hz in PSRAM und bleibt auch bei WebUI-Seitenwechseln aktiv.

Details: `docs/weight-diagnostics.md`.

---

## Kalibrierung

Implementiert:

- Tara
- Kalibrierung mit bekanntem Gewicht
- Speicherung des Faktors in Preferences/NVS
- Wiederladen beim Boot

Die T4-S3 speichert den Faktor als `raw/g`, während `HX711_ADC` intern auf Faktor `1.0` bleibt.

---

## Aktueller Integrationsstand

Die echte HX711-Messung ist vollständig integriert in:

- HMI
- WebUI
- Autodetect / Auto-Tara / Save
- Shot-Erkennung
- Flowrate
- BLE-Gewichtsübertragung
- Diagnose-CSV

Der Simulator ist nur noch Fallback, wenn kein gültiger HX711-Wert verfügbar ist.

---

## Aktuelle Abstimmung

Wichtige Parameter des getesteten Standes:

| Funktion | Wert |
|---|---:|
| HX711 | 80 Hz |
| Stable-Delay | `700 ms` |
| Display-Deadband | `0.08 g` |
| Gefäß-Auto-Tara-Bestätigung | `800 ms` |
| Shot-Sampling | `100 ms` / 10 Hz |
| Shot-Telemetrie WebUI | `200 ms` / 5 Hz |

Der aktuelle mechanische und softwareseitige Stand liefert schnelle Lastsprünge, reproduzierbare Auto-Tara und einen stabilen Nullpunkt. Bei späteren Änderungen an Wägeplatte, Biegebalken oder Kabelführung sollte zuerst die Mechanik erneut geprüft werden, bevor Filterparameter verändert werden.
