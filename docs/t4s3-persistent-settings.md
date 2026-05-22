# T4-S3 Persistente Einstellungen

Dieser Stand speichert zunächst nur UI-nahe Einstellungen des T4-S3-LVGL-HMI in NVS/Preferences.

Namespace:

```text
t4s3ui
```

Gespeichert werden:

```text
timeoutMin   Bildschirmtimeout in Minuten: 1 / 5 / 10 / 30
autodetect   Autodetect AN/AUS
selST        aktuell gewählter Siebträger: 0..3
targetST0    Sollgewicht Bodenloser ST in Zehntelgramm
targetST1    Sollgewicht 1er-Siebtraeger in Zehntelgramm
targetST2    Sollgewicht 2er-Siebtraeger in Zehntelgramm
targetST3    Sollgewicht Custom ST in Zehntelgramm
targetStep   Schrittweite für Sollgewicht in Zehntelgramm: 0,1 / 0,5 / 1,0 g
```

Wichtig:

- Siebträger-Gewichte werden nicht gespeichert.
- Siebträger dienen auf der Single-Dose-Waage nur als Profil für das Sollgewicht.
- Auto-Tara / Autodetect basiert später auf Gefäßen, nicht auf Siebträger-Gewichten.
- Gefäßgewichte, Kalibrierwerte, Wartungszeiten und Statistikwerte werden in späteren Schritten ergänzt.

Standardwerte:

```text
Bodenloser ST    18,0 g
1er-Siebtraeger   9,0 g
2er-Siebtraeger  18,0 g
Custom ST        18,0 g
Schrittweite      0,5 g
Timeout           5 min
Autodetect        AN
```

## Demo-Statistikwerte

Zusätzlich werden jetzt die im T4-S3-Prototyp angezeigten Statistikwerte gespeichert:

```text
shotsTotal    Shots gesamt
shotsMach     Shots seit Reinigung Kaffeemaschine
shotsGrind    Shots seit Reinigung Kaffeemuehle
shotsFilter   Shots seit Filterwechsel

gramsTotal    Mahlgut gesamt in Zehntelgramm
gramsMach     Mahlgut seit Reinigung Kaffeemaschine in Zehntelgramm
gramsGrind    Mahlgut seit Reinigung Kaffeemuehle in Zehntelgramm
gramsFilter   Mahlgut seit Filterwechsel in Zehntelgramm
```

Diese Werte sind weiterhin Prototyp-/Demo-Werte, bis die echte Waagenlogik und HX711-Anbindung aktiv sind.

## Wartungszeiten

Die T4-S3-Wartungsseite speichert die letzten Wartungszeitpunkte als Epoch-Zeit in Sekunden:

```text
lastKaffee   letzter Reset Kaffeemaschine
lastMuehle   letzter Reset Kaffeemuehle
lastFilter   letzter Reset Filterwechsel
```

Verwendete Intervalle aus der bestehenden Projektlogik:

```text
Kaffeemaschine reinigen: 10 Tage    = 864000 Sekunden
Muehle reinigen:          28 Tage    = 2419200 Sekunden
Filterwechsel:            12 Wochen  = 7257600 Sekunden
```

Die Anzeige verwendet `in` für noch nicht fällige Wartung und `seit` für überfällige Wartung.
Das Format ist zum Beispiel:

```text
Kaffeemaschine: in      9 Tagen, 12:35:24
Filter: seit            2 Tagen, 22:34:13
```
