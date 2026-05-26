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

- Live-Anzeige
- Reaktion auf Auflegen
- Shot-Dynamik

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

# Bewegungs- und Stabilitätserkennung

Derzeit:

- Vergleich von FAST gegen DISPLAY
- Schwellwert-basierte Erkennung
- zusätzlicher Zeitfilter

Status:

- funktioniert bereits gut
- Übergänge können später noch weicher gemacht werden


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

# Nächster Schritt

## Ziel

Echte HX711-Gewichtsanzeige auf der Waagen-Seite verwenden.

Der bisherige Simulator soll schrittweise ersetzt werden.

## Geplanter Ablauf

1. Anzeigegewicht auf der Waagen-Seite an DISPLAY koppeln
2. bestehende UI-Logik weiterverwenden
3. Stabilitätslogik später für:
   - Auto-Tara
   - Gefäß-Erkennung
   - Save-Freigabe
   verwenden


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
