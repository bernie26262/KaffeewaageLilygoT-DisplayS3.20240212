# B4a.2 - Passive Controller-Anbindung in `main.cpp`

Ausgangsbasis: Commit `d4ca1fe` (`feature/grind-by-weight`, nach B4a.1).

## Umfang

- `GrinderCycleGate` einmalig mit Referenz auf `grinderProfilePreview` erstellen.
- Nur **neue** HX711-Daten an den Detektor und danach an das Gate geben;
  dieselbe Stabilitaetsinformation aus `GrinderWeightStability` verwenden.
- Zuerst `grinderProfilePreview.tick()`, dann `grinderCycleGate.onWeightSample()`
  und nach dem Service **in jedem Loop** `grinderCycleGate.tick()` ausfuehren.
- Das Sample noch vor einer moeglichen blockierenden Auto-Tara weitergeben.
  Nach Tara ist erst ein **neues** HX711-Sample ein gueltiger Nullwert.
- Bestehendes `GRINDER_WEIGHT_MONITOR_LOG=1` um eine zweite
  `[GBW B4a.2]`-Statuszeile im Abstand von ca. 2 s erweitern.

## Erwartete serielle Ausgabe

`[GBW B4a.2] state=0 gateWeight=0.00 g fresh=1 stable=1 ready=1 profile=... target=... g consumed=0 motorRequest=0 (SSR OFF)`

`state=0` bedeutet `Idle`. Das ist **immer** der erwartete Gate-Zustand
in dieser Phase. `ready=1` bestaetigt nur die **Profilerkennung** und ist
noch kein durch die B4a.1-Gate-Schutzbedingungen freigegebener Start.
`target` ist der momentan im Detektor gespeicherte Profilwert; Aenderungen
an Sollgewichten waehrend des Betriebs werden derzeit nicht ueberall
synchronisiert. **Vor Freischalten eines Startbefehls** muss das getrennt
abgeglichen werden.

## Verbotene Verbindungen

- Kein `requestStart()`/`requestCancel()` aus HMI oder WebUI.
- Kein `digitalWrite()` in der Integrationsschicht.
- Kein `logicRunning()`/`relayRequested()`-Wert an GPIO16 oder ein Relais.
- Keine weiteren Aenderungen an HMI, Tara, Skip, Profileinmessung, WebUI.

## Test

- Bestehende 10 Host-Tests sowie
  `test_host/grinder_passive_wiring_b4a2_test.cpp` mit `g++`.
- Auf Windows: `git apply --check`, `git apply`, `git diff --check`,
  dann PlatformIO-Build und USB-Hardware-Regressionstest.
- Im Serial Monitor muessen `state=0`, `consumed=0`, `motorRequest=0`
  auch beim Erkennen/Tarieren/Abheben durchgehend erhalten bleiben.
- Freigabe fuer den naechsten Schritt erst nach bestaetigtem Test;
  vorher kein Commit und kein Motorbetrieb.
