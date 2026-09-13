# BLE- und Shot-Waagen-Integration

## Zielbild

Die T4-S3-Waage besitzt zwei fachlich getrennte Betriebsarten:

- **Single Dose** für Bohnen, Gefäßerkennung, Auto-Tara und Save Dose
- **Shot-Waage** für Espresso-Bezug, Maschinen-Tara, Zeit, Gewicht, Flowrate und WebUI-Graph

BLE bleibt in beiden Modi aktiv und verbunden. Nur die Ausführung von Maschinenkommandos wird im Single-Dose-Modus gesperrt.

## WeighMyBru-GATT

Gerätename:

```text
WeighMyBru
```

UUIDs:

| Funktion | UUID |
|---|---|
| Service | `6E400001-B5A3-F393-E0A9-E50E24DCCA9E` |
| Gewicht | `6E400002-B5A3-F393-E0A9-E50E24DCCA9E` |
| Kommando | `6E400003-B5A3-F393-E0A9-E50E24DCCA9E` |
| zusätzlicher einfacher Gewichtswert | `6E400004-B5A3-F393-E0A9-E50E24DCCA9E` |

Gewicht wird mit 5 Hz als 20-Byte-WeighMyBru-Paket gesendet. Der Client muss Notifications auf dem Gewicht-Characteristic aktivieren.

## Kommandos

Der Decoder unterstützt:

| Code | Kommando |
|---|---|
| `0x01` | TARE |
| `0x02` | TIMER_START |
| `0x03` | TIMER_STOP |
| `0x04` | TIMER_RESET |

Im realen Test mit Gaggiuino wurde nur `TARE` empfangen. `START`, `STOP` und `RESET` bleiben implementiert, werden aber von der aktuell verwendeten Gaggiuino-WeighMyBru-Integration nicht gesendet.

## BLE-Lebenszyklus

- Startverzögerung: 5 Sekunden nach Boot
- vor BLE-Init wird WLAN-Modem-Sleep aktiviert
- normale Sendeleistung `ESP_PWR_LVL_P3`
- nach Disconnect wird Advertising sofort neu gestartet
- Wechsel zwischen Single Dose und Shot trennt BLE nicht

Farben in HMI und WebUI:

- grau/durchgestrichen: BLE aus oder nicht verfügbar
- blau: BLE aktiv/Advertising, nicht verbunden
- grün: Client verbunden

Die Farbe sagt nichts über die Steuerfreigabe aus. Im Single-Dose-Modus kann BLE grün sein, während Maschinenkommandos trotzdem gesperrt sind.

## Shot-Session

Zustände:

```text
Idle -> Armed -> Running -> Completed
```

### Armed

Tara im Shot-Modus aktiviert die Flüssigkeits-Erkennung. Nach `500 ms` Beruhigungszeit wird ab `0,25 g` ein Startkandidat beobachtet. Die reine Gewichtsschwelle reicht bewusst nicht mehr aus, weil die Vibrationspumpe im realen Aufbau scheinbare Gewichtsänderungen von mehr als `0,45 g` erzeugen kann.

Ein Start wird erst bestätigt, wenn zusätzlich ein plausibler positiver Trend vorliegt:

- Bestätigungsgewicht mindestens `0,45 g`
- Kandidat mindestens `250 ms`
- Trendfenster ca. `1,2 s`
- auswertbare Zeitspanne mindestens `0,9 s`
- Nettozuwachs mindestens `0,15 g`
- Regressionssteigung mindestens `0,08 g/s`
- RMSE zum linearen Trend maximal `0,05 g`

Diese Kriterien wurden gegen reine Pumpenläufe und mehrere reale Shots geprüft. Die Pumpenläufe starteten keine Session; reale Flüssigkeitszunahme wurde weiterhin erkannt.

Ohne Start endet Armed nach 45 Sekunden. Ein zuvor abgeschlossener Shot bleibt dabei erhalten.

### Running

Beim Start wird der vorherige Sample-Puffer gelöscht und eine neue Session-ID vergeben. Samples werden mit 10 Hz aufgezeichnet.

Zusätzliche False-Start-Sicherung:

- nur in den ersten `6 s`
- Peakgewicht unter `2 g`
- Flow höchstens `0,05 g/s`
- wenn das Gewicht mindestens `1 s` wieder bei höchstens `0,12 g` liegt, wird die Session als Fehlstart verworfen und erneut `Armed`
- ein Gaggiuino-`TARE` kann einen sehr frühen Fehlstart ebenfalls zurücksetzen, wenn die Session noch leicht/flowfrei ist und das aktuelle Gewicht höchstens `0,20 g` beträgt

Damit bleibt ein Phantom-Shot nicht mehr unbegrenzt aktiv, falls die Pumpenvibration trotz Trendprüfung einmal einen Start auslösen sollte.

Automatischer Stop:

- Mindestdauer 8 Sekunden
- mindestens 4 g Peakgewicht
- relevanter Zuwachs ab 0,20 g
- Stop nach 3 Sekunden ohne relevanten weiteren Zuwachs

Die effektive Endzeit wird auf den letzten relevanten Gewichtszuwachs zurückgesetzt, sodass die dreisekündige Bestätigungswartezeit nicht zur angezeigten Shot-Zeit zählt.

### Completed

Endzeit, Endgewicht und Graph bleiben stehen. Tara bereitet nur den nächsten Shot vor; der alte Verlauf wird erst beim tatsächlichen Start überschrieben.

## Zeitdefinition

Die Waage misst die Zeit ab dem ersten **plausibilisierten Flüssigkeitszuwachs** in der Tasse.

Das ist bewusst nicht exakt gleichbedeutend mit dem optisch sichtbaren ersten Tropfen: Bei sehr langsamem Tropfen kann die Trendprüfung erst etwas später genügend Signal sammeln. Sie verhindert im Gegenzug, dass reine Pumpenvibration als erster Tropfen interpretiert wird.

Da Gaggiuino im getesteten WeighMyBru-Betrieb kein `TIMER_START` sendet, kann die Waage Pumpenstart und Pre-Infusion nicht direkt übernehmen. Die Maschinenzeit in Gaggiuino kann daher länger sein als die Waagenzeit.

## RAM-Puffer

```text
1.200 Samples
10 Hz
maximal 120 Sekunden
```

Gespeichert werden Zeit, Gewicht und Flowrate. Es gibt bewusst keine dauerhafte Shot-Historie auf der Waage. Der letzte Shot geht bei ESP-Neustart verloren.

## Flowrate

Die Flowrate wird aus der Steigung der Gewichtskurve berechnet:

- lineare Regression über die letzten 2,5 Sekunden
- mindestens 1,2 Sekunden Zeitspanne
- mindestens 10 Samples
- EMA-Glättung mit Alpha 0,15
- Deadband 0,05 g/s
- Begrenzung auf 20 g/s

Diese stärkere Glättung ist bewusst gewählt, weil die Ableitung kleine Gewichtsschwankungen stark verstärkt.

## WebUI-Graph und Live-Telemetrie

Die WebUI verwendet zwei Ebenen:

- Full State außerhalb eines Shots alle `500 ms`
- während `armed/running` kompaktes `shot_telemetry` alle `200 ms`; der große Full State wird in dieser Phase auf `1000 ms` reduziert

`shot_telemetry` enthält unter anderem Session-ID, Zustand, Shot-Gewicht, Zeit, Peak, Flow und Sampleanzahl. Dadurch aktualisieren sich Gewicht und Flow sichtbar flüssiger, ohne den vollständigen AppState mit 5 Hz zu übertragen. Der Browser führt die Timeranzeige zwischen den Telemetriepaketen weiter.

Shot-Samples werden weiterhin separat über `/api/shot/samples` in Paketen bis 120 Punkte geladen.

Vorteile:

- kleine reguläre WebSocket-Nachrichten
- flüssige Shot-Anzeige mit ca. 5 Hz Telemetrie
- Browser-Reconnect kann kompletten RAM-Shot nachladen
- Desktop und Mobilgerät bleiben reaktiv
- keine Flash-Schreibzugriffe

## Bekannte Einschränkung Gaggiuino

Der Verbindungsaufbau ist grundsätzlich möglich, und Reconnects wurden beobachtet. Das Aus- und Wiedereinschalten der Bluetooth-Scale-Funktion kann jedoch auf Gaggiuino-Seite zu einem Geräte-Neustart führen. Im Waagenlog wurden dabei keine entsprechenden Abstürze erkannt.

Daher im Alltag:

- Bluetooth-Scale-Funktion auf der Maschine eingeschaltet lassen
- BLE auf der Waage nicht als Bedienoption abschaltbar machen
- Reconnect-Probleme nicht durch häufiges Umschalten der Maschinenfunktion provozieren
