# B4c.2b – Vorbereitete SSR-Ausgangsgrenze mit getrenntem Prüfstand-Modus

**Basis:** Repository-ZIP `KaffeewaageESP_GrindByWeight_Modern.20261025_1513.zip`,
Branch `feature/grind-by-weight`, zuletzt gemeldeter Commit `93d5a53`.

## WICHTIG: Standard-Firmware darf die Mühle weiterhin nicht einschalten

In `platformio.ini` bleiben diese beiden Schalter auf **0**:

```ini
-DGRINDER_SSR_PHYSICAL_ENABLE=0
-DGRINDER_SSR_BENCH_ENABLE=0
```

Der erste Schalter bleibt auch in B4c.2b **fest auf 0 verriegelt**. Ein
anderer Wert erzeugt absichtlich einen Buildfehler. Der zweite Schalter ist
neu: `GRINDER_SSR_BENCH_ENABLE=1` erlaubt erstmals einen GPIO16-LOW-Pegel,
**ausschließlich für einen Prüfstand mit vollständig und fachgerecht
getrennter 230-V-Lastseite des SSR**. Mit der angeschlossenen Mühle darf
eine solche Test-Firmware **nicht hochgeladen oder betrieben werden**.
Eine angeschlossene 5-V-Steuerseite allein ist kein Ersatz für diese Trennung.

Der B4c.2b-Bench-Modus ist **keine Betriebsfreigabe**. Eine unabhängige
Abschaltung bei steckengebliebener CPU beziehungsweise dauerhaftem
GPIO16-LOW wurde noch nicht implementiert.

## Software-Änderungen

1. Nur `src/grinder_relay.cpp` schreibt GPIO16. Die standardmäßige
   `grinderRelayApplyGuard()`-Ausführung schreibt ausschließlich HIGH.
   Der explizite Bench-Build kann LOW nur bei `guard.permitted()` anfordern.
2. Die bisherige bedingungslose `grinderRelayForceOff()`-Anweisung am Anfang
   von `loop()` entfällt. Ein künftig eingeschalteter Ausgang würde sonst
   bei jedem Schleifendurchlauf kurzzeitig ausgeschaltet.
3. Die asynchrone `grinderRelayForceOff()`-Anweisung setzt eine
   Abschaltverriegelung und HIGH in derselben, kernübergreifend geschützten
   kritischen Sektion. Ein parallel ausgeführtes `applyGuard()` kann daher
   nach diesem Abbruch keinen verspäteten LOW-Pegel mehr schreiben.
4. Die WebUI-Fehlerbehandlung setzt zuerst das atomare Abbruchsignal und
   erzwingt anschließend unmittelbar den verriegelten HIGH-Zustand.
5. OTA-Upload und Neustart verwenden die strengere
   `grinderRelayForceOffUntilRestart()`-Sperre. Sie bleibt auch nach einer
   kompletten Entnahme und einer erneuten Vorbereitung bestehen und wird
   erst durch einen echten ESP-Neustart aufgehoben.
6. Eine normale WebUI-Abschaltverriegelung kann erst zurückgesetzt werden,
   nachdem Controller und Gefäßerkennung wieder unvorbereitet sind, die
   Ausgangs-Schutzlogik zurückgesetzt wurde und kein neuer WebUI-Abbruch
   aussteht.
7. Die OTA-Neustartanforderung und ihr Zeitstempel werden nun atomar
   zwischen Webserver- und Hauptloop-Task übertragen.

## Bereits gemessene Hardware-Basis

- Ohne externe RC-Beschaltung: kurze LOW-Impulse an GPIO16 beim Kaltstart
  und beim Hardware-Reset mit dem PeakTech 1401 nachgewiesen.
- Bei zehn Kaltstarts sowie zehn Hardware-Resets mit angeschlossener Mühle
  **keine wahrnehmbare Motorbewegung**. Das belegt nicht, dass das SSR unter
  allen elektrischen Bedingungen niemals kurzzeitig leitend wird.
- Software-Neustarts über WebUI/PowerShell zeigten bei den bisherigen
  Oszilloskopmessungen keine vergleichbaren LOW-Triggerereignisse.

## Host-Tests (Linux, macOS oder Windows mit g++)

```sh
g++ -std=c++11 -Wall -Wextra -Werror -Isrc -Itest_host/fakes \
  src/grinder_relay.cpp src/grinder_output_guard.cpp \
  test_host/grinder_relay_b4c2a_test.cpp -o grinder_relay_b4c2a_test
./grinder_relay_b4c2a_test

g++ -std=c++11 -Wall -Wextra -Werror -DGRINDER_SSR_BENCH_ENABLE=1 \
  -Isrc -Itest_host/fakes src/grinder_relay.cpp \
  src/grinder_output_guard.cpp \
  test_host/grinder_relay_b4c2b_bench_test.cpp -o grinder_relay_b4c2b_bench_test
./grinder_relay_b4c2b_bench_test
```

Im OFF-Test wird jeder GPIO-LOW-Schreibversuch als Fehler gewertet.
Der Bench-Test simuliert ein gültiges LOW, WebUI-Abbruch, rearm nach
Entnahme, OTA-Sperre bis Neustart und HIGH-vor-OUTPUT beim ESP-Neustart.
Zusätzlich die bestehenden Guard-/Controller-Tests ausführen.

Die Host-Tests prüfen die logischen Aufrufreihenfolgen, **nicht** den
asynchronen Zeitablauf zweier echter ESP32-Kerne und **nicht** die elektrische
Schaltfunktion des SSR.

## Empfohlene weitere Prüfreihenfolge

1. **Jetzt:** OFF-only-Build mit unverändertem Flag `0` bauen; prüfen, dass
   kein LOW in der Produktions-Firmware erreichbar ist. Die AC-Lastseite
   darf dabei angeschlossen bleiben, der Bench-Modus darf NICHT aktiv sein.
2. **Erst nach erneuter sicherer Lasttrennung:** geeignete Niedervolt-Messung
   an GPIO16 und der SSR-Steuerseite vorbereiten. Niemals eine gewöhnliche
   geerdete Oszilloskop-Masse an den 230-V-Kreis anschließen.
3. `GRINDER_SSR_BENCH_ENABLE=1` nur im getrennten Prüfstand bauen und flashen.
   Regulären Mahlstart, Zielgewicht, Abbruch, Nachmahlen, WebUI-Abbruch,
   OTA-Upload und Neustart kontrollieren. Jede Störung muss HIGH ergeben.
4. `GRINDER_SSR_BENCH_ENABLE=0` wiederherstellen und den OFF-only-Build
   erneut flashen und verifizieren **bevor** die AC-Last verbunden wird.
5. Unabhängige Hardware-Fail-OFF-Lösung für dauerhaftes LOW/CPU-Hänger
   festlegen, prüfen und erst danach einen gesonderten Produktiv-Patch
   für die echte SSR-Ansteuerung entwickeln.

**Nicht abgeprüft in B4c.2b:** Hardware-Watchdog, Brownout, hängende CPU,
SSR-Eingangsstrom/Logikpegel, elektrische AC-Ausgangsreaktion auf kurze
Boot-Pulse, Dauerlauf eines defekten SSR. Hauptschalter zugänglich halten.
