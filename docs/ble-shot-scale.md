# BLE- und Shot-Waage

## Geltungsbereich und Teststatus

Diese Datei beschreibt die aktuelle Legacy-TFT-Implementierung dieses Branches.

Praktisch getestet und freigegeben ist:

- WeighMyBru-kompatible BLE-Verbindung
- Zusammenspiel mit Gaggiuino
- Tara durch die Maschine
- automatische Shot-Erkennung
- Shot-Zeit, Gewicht und geglaettete Flowrate
- HMI-/WebUI-Synchronisierung
- Live-Graph und letzter Shot im RAM

Nicht praktisch getestet ist GaggiMate, weil derzeit keine passende Maschine verfuegbar ist. GaggiMate-spezifische Aenderungen sollen deshalb erst mit Testhardware erfolgen. Acaia- oder Bookoo-Protokolle sind aktuell nicht Teil des freigegebenen Funktionsumfangs.

## Betriebsmodi

Die Waage hat zwei fachlich getrennte Modi:

### Single Dose

- normale Gefaess-/Autodetect- und Save-Dose-Logik
- BLE-Gewicht kann weiterhin bereitgestellt werden
- Maschinenkommandos werden nicht auf die Shot-Session angewendet

### Shot-Waage

- keine Gefaess-Autodetect-Logik
- Tara bereitet die automatische First-Drop-Erkennung vor
- Shot startet und endet anhand der Gewichtsentwicklung
- Zeit, Gewicht und Flow werden auf HMI und WebUI angezeigt

Die WebUI-Hauptnavigation synchronisiert den Modus mit dem lokalen HMI. Daten und Einstellungen verlassen den Shot-Modus. Ein laufender Shot wird dabei sauber beendet, eine nur wartende Erkennung wird abgebrochen, und der letzte abgeschlossene Verlauf bleibt erhalten.

## Modulaufteilung

### `src/ble_scale.cpp/.h`

Zustaendig fuer:

- BLE-Initialisierung und Advertising
- WeighMyBru-GATT-Service
- Gewichtspakete
- Empfang von Maschinenkommandos
- Verbindungs- und Ratenstatistik
- kurzes Ringlog im RAM

### `src/shot_session.cpp/.h`

Zustaendig fuer:

- Zustaende `idle`, `armed`, `running`, `completed`
- First-Drop-Erkennung
- automatisches Shot-Ende
- Shot-Zeit und Endgewicht
- Messpunktpuffer
- Berechnung und Glaettung der Flowrate

### `src/main.cpp`

Koordiniert:

- aktiven Waagenmodus
- Tara und reale Waagenwerte
- BLE-Kommandos
- Shot-Ereignisse
- TFT-Aktualisierung
- AppState und WebSocket-Broadcasts

### `src/coffee_web.cpp`

Stellt bereit:

- Shot-Bedienseite
- Status ueber WebSocket
- BLE-Diagnose
- inkrementellen Messpunkt-Endpunkt
- Canvas-Graph fuer Gewicht und Flow

## BLE-Konfiguration

Die Funktion wird ueber Build-Flags in `platformio.ini` gesteuert:

```ini
-DENABLE_BLE_SCALE=1
-DBLE_SCALE_START_DELAY_MS=5000UL
-DBLE_SCALE_LOW_POWER=0
; -DBLE_SCALE_VERBOSE_LOGS=1
```

Bedeutung:

- BLE ist aktiviert.
- Der Start erfolgt ca. 5 Sekunden nach dem Boot.
- Aktuell wird nicht die reduzierte Sendeleistung verwendet.
- Rohes serielles BLE-Logging bleibt standardmaessig aus.

Das Geraet advertised als:

```text
WeighMyBru
```

## GATT-Schnittstelle

```text
Service:              6E400001-B5A3-F393-E0A9-E50E24DCCA9E
Weight READ/NOTIFY:   6E400002-B5A3-F393-E0A9-E50E24DCCA9E
Command WRITE/WRITE_NR: 6E400003-B5A3-F393-E0A9-E50E24DCCA9E
Simple weight READ:   6E400004-B5A3-F393-E0A9-E50E24DCCA9E
```

Das WeighMyBru-Gewichtspaket ist 20 Byte lang. Das Gewicht wird mit zwei Nachkommastellen und Vorzeichen uebertragen; Byte 19 enthaelt die XOR-Pruefsumme der ersten 19 Byte.

Gewichtsmeldungen werden bei bestehender Verbindung mit maximal 5 Hz erzeugt. Die Maschine erhaelt den aktuellen Waagenwert unabhaengig von der langsameren Flow-Berechnung.

## Maschinenkommandos

Unterstuetzt werden:

| Wert | Kommando | Verhalten im Shot-Modus |
|---:|---|---|
| `0x01` | TARE | Waage tarieren und Shot-Erkennung vorbereiten |
| `0x02` | START | Shot extern starten, sofern er nicht bereits laeuft |
| `0x03` | STOP | laufenden Shot extern beenden |
| `0x04` | RESET | Shot-Erkennung erneut vorbereiten |

Akzeptiert werden sowohl Ein-Byte-Kommandos als auch das getestete WeighMyBru-/Gaggiuino-Systempaket.

Im Single-Dose-Modus werden Maschinenkommandos nicht ausgefuehrt und als `ignoriert - Single Dose aktiv` protokolliert. START und STOP melden nur dann `ausgefuehrt`, wenn sich der Zustand wirklich geaendert hat; sonst erscheint `ohne Zustandsaenderung`.

Der aktuelle Gaggiuino-Praxistest basiert im regulaeren Ablauf vor allem auf TARE und der anschliessenden automatischen First-Drop-Erkennung. START/STOP/RESET bleiben als kompatible externe Steuerbefehle vorhanden.

## Automatische Shot-Erkennung

Nach Tara wechselt die Session auf `armed`.

Aktuelle Schwellwerte:

| Funktion | Wert |
|---|---:|
| Beruhigungszeit nach Tara | 500 ms |
| Bereitschafts-Timeout | 45 s |
| Startkandidat | 0,25 g |
| sichere Startbestaetigung | 0,45 g fuer 250 ms |
| Abbruch eines Startkandidaten | unter 0,12 g |
| minimale Shot-Dauer fuer Auto-Ende | 8 s |
| minimales Spitzengewicht fuer Auto-Ende | 4,0 g |
| relevanter weiterer Gewichtszuwachs | 0,20 g |
| Auto-Ende ohne weiteren Zuwachs | 3 s |

Beim automatischen Ende wird die Session auf den Zeitpunkt des letzten relevanten Gewichtszuwachses gekuerzt. Dadurch landen nachlaufende Stillstandssekunden nicht im finalen Timer und Graphen.

## Messpunkte und Flowrate

Die Session speichert alle 100 ms einen Messpunkt, also 10 Hz. Der Puffer umfasst maximal 1200 Punkte beziehungsweise 120 Sekunden.

Ein Punkt besteht aus:

```text
Zeit seit Shot-Start in ms
Gewicht in g
Flow in g/s
```

Die Flowrate ist die Steigung der Gewichtskurve. Zur Rauschunterdrueckung werden verwendet:

- lineare Regression ueber bis zu 2,5 Sekunden
- mindestens 10 Punkte und 1,2 Sekunden Zeitspanne
- anschliessende exponentielle Glaettung
- Deadband unter 0,05 g/s
- Begrenzung auf maximal 20 g/s

Diese Glaettung betrifft nur Anzeige und gespeicherte Flow-Punkte. Die BLE-Gewichtsmeldungen bleiben davon unabhaengig.

## RAM-Lebensdauer des letzten Shots

Der letzte abgeschlossene Shot bleibt im RAM erhalten:

- Tara allein loescht ihn nicht.
- Seitenwechsel loescht ihn nicht.
- Browser-Neuladen kann ihn erneut abrufen.
- Erst der sicher erkannte Start eines neuen Shots ersetzt den alten Verlauf.
- Ein ESP-Neustart loescht Verlauf und Diagnose-Log.

Es gibt bewusst noch kein dauerhaftes Shot-Archiv in NVS oder einem externen Speicher.

## WebSocket-State und Messpunkt-API

Der WebSocket-State liefert die kompakten Livewerte unter `shot`, unter anderem:

```text
session_id
state
armed
running
completed
elapsed_ms
current_weight_g
peak_weight_g
final_weight_g
current_flow_g_s
sample_count
sample_buffer_full
```

Die Messpunkte werden getrennt geladen:

```text
GET /api/shot/samples?session=<id>&from=<index>
```

Beispielstruktur:

```json
{
  "session_id": 12,
  "from": 160,
  "total": 245,
  "running": true,
  "completed": false,
  "samples": [
    [16000, 24.531, 1.842]
  ],
  "end_session_id": 12
}
```

Pro Antwort werden maximal 160 Punkte uebertragen. Der Browser fordert fehlende Bloecke nach und zeichnet den Graphen neu. `end_session_id` dient als zweite Konsistenzpruefung, falls waehrend der Antwort bereits eine neue Session begonnen hat.

## WebUI-Graph

Der Graph zeigt:

- Gewicht in Gruen
- Flow in Blau
- Zeitachse in Sekunden
- linke Y-Achse fuer Gramm
- rechte Y-Achse fuer g/s
- automatische Skalierung

Die WebUI ersetzt den alten Graphen erst beim wirklichen Start einer neuen Session. Bei einem automatischen Shot-Ende kann sich die finale Punktanzahl durch das Abschneiden der Stillstandsphase verringern; die WebUI erkennt dies ueber `sample_count` und laedt den korrigierten Verlauf neu.

## HMI-/WebUI-Synchronisierung

Zuordnung der WebUI-Hauptbereiche zum Legacy-HMI:

| WebUI | HMI-Seite | Waagenmodus |
|---|---:|---|
| Waage | 0 | Single Dose |
| Shot-Waage | 3 | Shot |
| Daten | 14 | Single Dose |
| Einstellungen | 1 | Single Dose |

Tiefere Einstellungsunterseiten der WebUI steuern das lokale HMI nicht.

## Backlight-Regel

WebUI- oder BLE-Fernbedienung soll das lokale Display nicht unnoetig aktivieren.

Kein Wakeup und keine Verlaengerung des Display-Timeouts durch:

- WebUI laden oder aktualisieren
- WebUI-Hauptseiten wechseln
- Einstellungen und Statusabfragen
- Tara oder andere WebUI-Kommandos
- BLE-Kommandos

Wakeup bleibt erlaubt durch:

- lokale Encoder-/Tasterbedienung
- signifikante Gewichtsveraenderung ab 30 g

Die Ziel-HMI-Seite wird bei Fernbedienung trotzdem im Hintergrund vorbereitet. Beim naechsten lokalen Wakeup erscheint daher bereits der passende Bereich.

## Diagnose

Das BLE-Modul haelt die letzten 12 Ereigniszeilen im RAM. Die WebUI zeigt unter anderem:

- Advertising gestartet
- Maschine verbunden/getrennt
- TARE/START/STOP/RESET mit Ergebnis
- Shot bereit, gestartet und beendet
- Ende der Shot-Bereitschaft

Gewichtsmeldungen werden nicht einzeln geloggt. Dadurch bleibt das Log lesbar und erzeugt keine dauernde Last.

Bei verfuegbarem USB kann fuer eine gezielte Fehlersuche aktiviert werden:

```ini
-DBLE_SCALE_VERBOSE_LOGS=1
```

Im normalen Legacy-Einbau bleibt dieser Schalter aus, weil der USB-/Serial-Zugang praktisch nicht verfuegbar ist und die WebUI-Diagnose der primaere Weg ist.

## Bewusste Grenzen und spaetere ToDos

- GaggiMate erst mit realer Testmaschine aufnehmen.
- Acaia-/Bookoo-Kompatibilitaet nur bei konkretem Bedarf untersuchen.
- kein dauerhaftes Shot-Archiv ohne vorherige Festlegung von Nutzen, Speicherort und Datenformat
- BLE-/Shot-Aenderungen immer gegen Single-Dose-Autodetect, HMI-Navigation, WebUI-Graph und Backlight-Regel regressionspruefen
