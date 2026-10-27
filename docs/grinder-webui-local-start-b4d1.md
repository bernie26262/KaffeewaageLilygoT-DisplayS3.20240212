# B4d.1 – Grind-by-Weight: Mahlstart nur lokal am TFT

## Ausgangsbefund

- Die WebUI enthaelt im Ausgangsstand keinen Start-/Nachmahlbutton.
- `src/coffee_web.cpp` uebertraegt Kommandos per `/ws`; unbekannte
  Kommandos werden von `handleCoffeeWebCommand()` abgelehnt.
- `GrinderCycleGate::requestStart()` wird nur durch den lokalen Encoder,
  `requestManualTopupPress()` nur durch die lokale rechte Taste aufgerufen.
- WebUI-Kommandos waehrend eines aktiven Mahlzyklus werden bereits durch
  die B4a.4-Sicherheitslogik abgewiesen und loesen einen Sicherheitsabbruch aus.

## B4d.1

- Sichtbarer Hinweis auf der Waagen-WebUI: Start und Nachmahlen ausschliesslich
  direkt am TFT.
- Zusaetzliche ausdrueckliche Sperre typischer Motor-Startkommandos vor der
  WebUI-Befehlsverarbeitung. Alle anderen unbekannten Kommandos bleiben durch
  die bestehende, explizite Befehlszuordnung abgelehnt.
- Host-Test prueft die Sperre und die unveraenderte Freigabe harmloser
  WebUI-Befehlsnamen. Es wurde kein Motor-Start- oder Top-up-Endpunkt eingefuehrt.

## Unveraendert

- TFT-Encoder/Right-Button-Start und -Abbruch.
- GrinderCycleGate, OutputGuard, Relay und Software-Heartbeat.
- B4a.4: Alle WebUI-Kommandos waehrend Grinding/ManualTopup/Settling
  fuehren weiterhin zum sicherheitsbedingten Abbruch/Reject.
- Netzwerkbasierte Einstellungen, Anzeige und passive WebSocket-Telemetrie.
- Keine eigenstaendige WebUI-Abbruchtaste: diese ist Teil der separaten
  spaeteren WebUI-Funktionsnacharbeit und wird nicht vorweggenommen.

## Host-Test

```sh
g++ -std=c++11 -Wall -Wextra -Werror -Isrc -Itest_host/fakes \
  src/grinder_controller.cpp src/grinder_cycle_gate.cpp \
  src/grinder_profile_detector.cpp \
  test_host/grinder_safety_b4a4_test.cpp -o /tmp/gbw_web_safety_test
/tmp/gbw_web_safety_test
```

## Sicherheit / Inbetriebnahme

Die Sperre ist eine Fernstart-Verhinderung, keine elektrische
Ausfallsicherung. Die SSR-Freigabeflags in `platformio.ini` bleiben
unveraendert (`GRINDER_SSR_PHYSICAL_ENABLE=0`, Bench separat gesetzt).
Eine Wiederinbetriebnahme des 230-V-Lastkreises wird durch B4d.1
nicht freigegeben.
