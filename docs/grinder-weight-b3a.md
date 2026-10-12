# Grind-by-Weight – Phase B3a: HX711-Stabilitaet, nur Diagnose

## Zweck

Dies ist eine bewusst **passive** Zwischenschicht vor B3b. Ausschliesslich
`LoadCell.update() == true` wird als frische HX711-Probe verarbeitet; wiederholt
abgerufene alte Werte zaehlen nicht als neue Messungen.

Eine Probe gilt als stabil nach mindestens 500 ms innerhalb von 0,35 g relativ
zum Anfang dieser Ruhephase und mindestens vier neuen Samples. Ausbleibende
Messungen fuer >1 s und ungueltige Werte entziehen den Stabil-Status.
Das sind Testparameter, die am echten HX711 noch validiert werden muessen.

Bei eingeschaltetem `GRINDER_WEIGHT_MONITOR_LOG=1` erscheint alle zwei Sekunden
im seriellen Monitor (`115200 Baud`) eine Zeile mit Gewicht, Fresh/Stable-Status
und Zahl der ruhigen Samples. Ohne frische Probe wird kein gueltiges Gewicht
behauptet.

## Bewusste Abgrenzung

- `GrinderProfileDetector` und `GrinderController` werden **noch nicht** mit
  diesen Daten gefuettert. Integration und HX711-Tara-Handshakes folgen in B3b.
- Keine Aenderung an der bisherigen HMI, WebUI, Profil-NVS oder
  Legacy-Autodetect-Funktion.
- Kein Aufruf von `LoadCell.tare*()`, keine Tasterbelegung und **keine
  Verbindung zu GPIO16**. Der SSR bleibt durch Phase A1 auf AUS.
- Die bestehende WebUI-Eigenschaft `appState.weight.stable = true` ist
  noch unberuehrt. Erst nach Hardwarevalidierung den neuen Status integrieren.

## Host-Test

```sh
g++ -std=c++11 -Wall -Wextra -Werror -Isrc \
  src/grinder_weight_stability.cpp test_host/grinder_weight_stability_test.cpp \
  -o grinder_weight_stability_test
./grinder_weight_stability_test
```

## Hardwaretest (ohne Muehlen-/Netzversorgung)

1. Board ausschliesslich an USB, Motor abgesteckt; `pio device monitor --port COM9 --baud 115200`.
2. Ohne Last 2–3 s Ruhe abwarten; `stable=1` bei gueltigen neuen Daten erwarten.
3. Siebtraeger vorsichtig auflegen; bei Gewichtswechsel zuerst `stable=0`,
   nach Ruhezeit wieder `stable=1` erwarten.
4. Falls `stable` nie 1 wird: aktuelle Stabilitaet und Rauschen untersuchen,
   keinen voreiligen Einfluss auf die Steuerung zulassen.
