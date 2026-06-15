# HX711 Architektur und Filterlogik

## Ziel

Die Kaffeewaage auf dem Lilygo T4-S3 AMOLED soll eine echte HX711-basierte Gewichtsmessung verwenden.

Wichtige Anforderungen:

- schnelle Reaktion beim Auflegen / Entfernen
- ruhige Endanzeige
- stabile Auto-Tara
- zuverlässige Gefäß-Erkennung
- spätere Shot-/Save-Logik
- ausreichend schnelle Reaktion für Espresso-Shots


---

# Hardware

## Aktueller Aufbau

ESP32 T4-S3 AMOLED:

- HX711 DT  -> GPIO 42
- HX711 SCK -> GPIO 41
- Versorgung über 3.3 V
- HX711 aktuell auf 80 Hz konfiguriert

Load Cell:

- derzeit noch provisorischer Kabelaufbau
- trotzdem bereits stabile Messergebnisse


---

# Erkenntnisse aus den bisherigen Tests

## 10 Hz

Vorteile:

- extrem ruhige Werte
- sehr geringe Schwankung

Nachteile:

- deutlich zu träge
- lange Einschwingzeit
- ungeeignet für dynamische Shot-Anzeige


## 80 Hz

Vorteile:

- sehr schnelle Reaktion
- gute Dynamik
- geeignet für Espresso-Anwendung

Nachteile:

- mehr Noise
- stärkere Filterung notwendig


Entscheidung:

→ 80 Hz bleibt die Zielkonfiguration.


---

# Aktuelle Messarchitektur

Die Messung verwendet mehrere getrennte Signalpfade.


## 1. RAW

Direkter HX711-Rohwert.

Verwendung:

- Debugging
- Analyse
- Kalibrierung

Keine Filterung.


---

## 2. FAST PATH

Schneller Gewichtspfad.

Verwendung:

- schnelle Single-Dose-Anzeige
- Reaktion auf Auflegen und Abheben
- Erkennung kleiner Bohnenänderungen

Eigenschaften:

- geringe Latenz
- leichte Glättung
- schnelle Reaktion

Aktuell:

- Median-Filter
- EMA mit höherem Anteil neuer Werte


---

## 3. STABLE PATH

Langsamer und ruhiger Gewichtspfad.

Verwendung:

- Stabilitätserkennung
- Auto-Tara
- Gefäß-Erkennung
- spätere Save-Freigabe

Eigenschaften:

- stärkere Glättung
- geringer Drift
- ruhiger Endwert


---

## 4. DISPLAY PATH

Adaptive Anzeige.

Die Anzeige schaltet abhängig vom Bewegungszustand zwischen FAST und STABLE um.

### Verhalten

#### Bewegung erkannt

Anzeige folgt FAST.

Ziel:

- schnelle Reaktion
- direkt sichtbare Änderung


#### Gewicht beruhigt sich

Übergangsphase Richtung STABLE.

Ziel:

- ruhige Anzeige
- kein Zittern


#### Stabil

Anzeige folgt STABLE.

Ziel:

- ruhiger Endwert
- gute Lesbarkeit


---


## 5. SHOT PATH

Für die Shot-Seite existiert ein eigener ruhiger Gewichtspfad. Er verwendet denselben Median-Vorfilter, schaltet bei Bewegung aber nicht unmittelbar auf den schnellen Single-Dose-Wert. Stattdessen folgt ein gleichmäßiger EMA mit schnellerem Nachziehen nur bei großen Abweichungen.

Verwendung:

- Gewichtsanzeige der Shot-Seite
- Gewichtsdaten an Gaggiuino während des Shot-Modus
- automatische First-Drop-Erkennung
- Stop-Erkennung
- Sample-Puffer und Flowrate

Dadurch bleiben Single Dose und Shot getrennt optimierbar:

- Single Dose: schnelle Reaktion auf einzelne Bohnen und Gefäßwechsel
- Shot: ruhige kontinuierliche Gewichtskurve

---

# Flowrate

Die Flowrate ist die Steigung der Gewichtskurve in g/s. Da eine Ableitung Messrauschen stark verstärkt, wird sie nicht aus zwei benachbarten Samples berechnet.

Aktueller Stand:

- Sample-Rate: 10 Hz
- Regressionsfenster: 2,5 s
- Mindestspanne: 1,2 s
- mindestens 10 Samples
- lineare Regression über Gewicht gegen Zeit
- anschließende EMA-Glättung mit Alpha `0.15`
- Deadband `0.05 g/s`
- Begrenzung auf `20 g/s`

Die Flowrate dient nur der Darstellung und dem Shot-Verlauf. Sie verändert weder die Rohmessung noch die Single-Dose-Stabilitätslogik.

---

# Bewegungs- und Stabilitätserkennung

Aktueller T4-S3-Stand:

- 80-Hz-HX711-Betrieb
- Median-Vorfilter
- FAST-Pfad für schnelle Reaktion
- STABLE-Pfad für ruhige Entscheidungen
- DISPLAY-Pfad für die Anzeige
- Bewegungserkennung über Differenzen im Rohwertbereich
- Stable-Delay aktuell `700 ms`
- Stable-Deadband der Anzeige aktuell `0.08 g`

Das zuletzt getestete Zielverhalten:

- größere Änderungen laufen schnell hinterher
- im stabilen Zustand bleibt die Anzeige ruhig bei `0,0 g`
- einzelne Bohnen mit etwas über `0,1 g` führen sichtbar zu einer Anzeigeänderung
- Autodetect/Auto-Tara verwenden weiterhin den ruhigen Stable-Pfad


---

# Kalibrierung

Bereits implementiert:

- Tara
- 100 g / 200 g Kalibrierung
- Speicherung des Kalibrierfaktors
- Wiederladen aus Preferences/NVS

Typischer Kalibrierwert:

- ca. 2790 raw/g


---

# Aktuelle Messergebnisse

Mit 200 g Referenzgewicht:

Typische Anzeige:

- 199.97 g
- 200.00 g
- 200.05 g

Das ist für:

- offenen Testaufbau
- 80 Hz
- WLAN aktiv
- AMOLED aktiv
- provisorische Verkabelung

bereits ein sehr gutes Ergebnis.


---

# Aktueller Integrationsstand

Die echte HX711-Messung ist vollständig in HMI, WebUI, Autodetect, Shot-Erkennung und BLE-Gewichtsübertragung integriert. Der Simulator dient nur noch als Fallback, wenn kein gültiger HX711-Wert verfügbar ist.

Die wichtigsten getrennten Verbraucher sind:

- DISPLAY/STABLE für Single Dose, Autodetect und Save
- SHOT für maschinennahe Anzeige, Shot-Erkennung und Flowrate

---

# Spätere mögliche Optimierungen

## Elektrik

- kürzere Kabel
- verdrillte Leitungen
- HX711 näher an die Load Cell

## Software

- weichere Settling-Kurve
- adaptive EMA
- Varianz-basierte Stabilitätserkennung
- getrennte Filter für:
  - Anzeige
  - Shot
  - Auto-Tara
  - Save


---

# Zwischenfazit

Der aktuelle Stand ist bereits deutlich über einem einfachen HX711-Testaufbau.

Die Waage zeigt jetzt:

- stabile Kalibrierung
- gute Reaktionszeit
- kontrollierte Glättung
- geringe Drift
- ruhige Endwerte
- adaptive Anzeige

Damit ist eine gute Basis geschaffen, um die echte Gewichtsmessung jetzt in die eigentliche Waagen-UI zu integrieren.

---

# Negative Null

Die Anzeige formatiert das Vorzeichen anhand des gerundeten Zehntelgramm-Werts. Dadurch wird ein Rohwert knapp unter Null nicht mehr als `-0,0 g`, sondern als `0,0 g` angezeigt. Erst wenn die gerundete Anzeige mindestens `-0,1 g` ergibt, wird das Minuszeichen dargestellt.

---

# Aktuelle Abstimmung

Der Stand nach dem letzten Test verwendet `kDisplayStableDeadbandGrams = 0.08f`. Dieser Wert ist ein guter Kompromiss:

- bei leerer Waage ruhige Anzeige
- keine dauernden Wechsel zwischen `-0,1 g`, `0,0 g` und `0,1 g`
- einzelne Bohnen über ca. `0,1 g` werden sichtbar erfasst

Wenn später ein anderer mechanischer Aufbau oder eine andere Wägezelle verwendet wird, ist dieser Wert ein wichtiger erster Abstimmparameter.
