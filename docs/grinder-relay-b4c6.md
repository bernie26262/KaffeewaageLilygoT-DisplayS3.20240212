# B4c.6a – Separater Software-Heartbeat-Wächter für den isolierten Prüfstand

## Status und Sicherheitsgrenze

- **Keine Freigabe für 230 V oder produktiven Mühlenbetrieb.**
- Die bisherige Compiler-Sperre `GRINDER_SSR_PHYSICAL_ENABLE != 0` bleibt
  unverändert bestehen.
- `GRINDER_SSR_BENCH_ENABLE=1` darf weiterhin **ausschließlich bei physisch
  vollständig getrennter Netz-/Lastseite** verwendet werden.
- Die zusätzliche Überwachung ist eine zweite *Software*-Task auf demselben
  ESP32. Sie ersetzt **weder eine unabhängige elektrische Abschaltung noch
  Schutz gegen ein dauerhaft leitendes SSR**.

## Verhalten

1. `grinderRelayInitOff()` setzt GPIO16 zuerst auf HIGH und startet nur
   im Bench-Build einen separaten FreeRTOS-Task auf Core 0.
2. Die Hauptschleife aktualisiert mit jedem zulässigen LOW-Ausgabebefehl
   einen Zeitstempel. Nur wenn der Wächter angelegt werden konnte, kann der
   Bench-Ausgang LOW werden (sonst fail-closed HIGH).
3. Der Wächter prüft alle 25 ms; fehlt während eines aktiven LOW-Pegels
   750 ms lang eine Aktualisierung, setzt er GPIO16 HIGH und verriegelt
   den Ausgang. Eine veraltete `guard.permitted()`-Freigabe darf danach
   keinen erneuten LOW-Pegel schreiben.
4. Nur der bereits vorhandene sichere Rücksetzpfad nach Entfernung des
   Siebträgers kann eine normale Verriegelung wieder freigeben. Die
   Neustartsperre während OTA bleibt erhalten.
5. Der bisherige `GrinderOutputGuard` mit 500-ms-Loop-Gap-Erkennung und
   den Laufzeitgrenzen bleibt unverändert als weitere Schutzschicht aktiv.

## Host-Tests

Mit C++11 und den vorhandenen Arduino-Fakes:

```sh
g++ -std=c++11 -Wall -Wextra -Werror -Isrc -Itest_host/fakes \
  src/grinder_relay.cpp src/grinder_output_guard.cpp \
  test_host/grinder_relay_b4c2a_test.cpp -o grinder_off_test
./grinder_off_test

g++ -std=c++11 -Wall -Wextra -Werror -DGRINDER_SSR_BENCH_ENABLE=1 \
  -Isrc -Itest_host/fakes src/grinder_relay.cpp \
  src/grinder_output_guard.cpp \
  test_host/grinder_relay_b4c2b_bench_test.cpp -o grinder_bench_test
./grinder_bench_test
```

Der zweite Test simuliert zusätzlich eine ausbleibende Hauptschleifen-
Aktualisierung und prüft Abschaltung und Verriegelung. Der Host-Test
simuliert *nicht* die FreeRTOS-Scheduling-Latenz, die elektrische Wirkung
des SSR oder einen tatsächlichen CPU-Hänger.

## Vor jeder weiteren Motorfreigabe offen

- PlatformIO-Build und echte Hardwareprüfung des Watchdog-Tasks im
  isolierten Bench-Betrieb (GPIO16, Oszilloskop).
- Gezielter *isolierter* Stall-/Reset-Test inklusive GPIO16-Pegel.
- Prüfung einer tatsächlich unabhängigen Hardware-Abschaltung bei
  blockiertem ESP und bei defektem SSR.
- Fachgerechte Bewertung von Netzverdrahtung, Schutzleiter, Isolation,
  Sicherung und Motoranlaufstrom gegenüber der SSR-Spezifikation.

Ohne diese Ergebnisse bleibt die Lastseite des SSR getrennt.
