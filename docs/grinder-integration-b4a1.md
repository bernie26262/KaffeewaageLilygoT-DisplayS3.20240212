# B4a.1 – Mahlstart-Sperre (nur Logik, ohne Hardwarefreigabe)

`GrinderCycleGate` liegt zwischen `GrinderProfileDetector` und
`GrinderController`. Diese Schicht hat **keinen GPIO-Zugriff** und ist noch
**nicht in `main.cpp` instanziiert**. Der SSR bleibt auf dem bestehenden
Boot-Aus-Zustand (GPIO16 HIGH); eine reale Motorfreigabe ist nicht implementiert.

## Startbedingungen

- Nur `ReadyAuto`, `ReadyNoFunnel`, `ReadyManual`, ohne Pause/laufende Tara.
- Gültige Profilauswahl und gespeichertes Sollgewicht 5..25 g.
- Neue, stabile, höchstens 1000 ms alte Messung nahe null (max. +/-1 g).
- Nur bewusstes `requestStart()`; einmaliger Start pro bestätigter Auflage.
- Controller darf ausschließlich von `Idle` aus starten, niemals automatisch
  bei `Complete`, `Fault` oder nach dem Neustart.
- B1-`ManualTopup` ist in dieser Phase nicht zugänglich.

## Sperren und Rückstellung

- Laufende Simulation: Verlust der Ready-Freigabe, Pause, Profil- oder
  Sollwertänderung -> verriegelter Fehlerzustand des B1-Controllers.
- Controller begrenzt zusätzlich Laufzeit und Alter/Gültigkeit der Messwerte.
- `Complete` bleibt gesperrt, auch bei kurzem `CheckingRemoval` mit Rückkehr
  zu `Ready*`. Rückstellung erst, wenn der Detektor nach einer bestätigten
  Entnahme und Tara wieder `WaitVessel` erreicht hat.
- Fehler bleiben bis zur **expliziten** Quittierung in `WaitVessel` verriegelt.
- `logicRunning()` ist nur die logische Controller-Anforderung, **niemals**
  ein GPIO-/SSR-Schaltbefehl.

## Spätere Einbindung (B4a.2/3)

- Bei neuen HX711-Daten erst `detector.onWeightSample()`, danach
  `gate.onWeightSample()` aufrufen. Stabilitätsinformationen vom bestehenden
  B3a-Modul verwenden.
- Regelmäßig `detector.tick()` und `gate.tick()` aufrufen.
- `requestStart()` nur nach absichtlicher Bedienhandlung (Tasterbelegung noch
  nicht festgelegt).
- Logikstatus und Fehlerbehandlung über das HMI sichtbar machen.
- Weder eine Relay-API noch ein Motor-Ausgang ist Bestandteil dieses Moduls.

## Host-Test

```sh
g++ -std=c++11 -Wall -Wextra -Werror -Isrc \
  src/grinder_controller.cpp src/grinder_profile_detector.cpp \
  src/grinder_cycle_gate.cpp test_host/grinder_cycle_gate_b4a1_test.cpp \
  -o grinder_cycle_gate_b4a1_test
./grinder_cycle_gate_b4a1_test
```

Der Host-Test ersetzt **nicht** PlatformIO-Build, USB-Hardwaretest und
spätere unabhängige SSR-Sicherheitsprüfung.
