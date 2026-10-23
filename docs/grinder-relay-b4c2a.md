# B4c.2a – Zentraler SSR-Ausgang, physisch weiterhin gesperrt

**Basis:** `c115f3f` auf `feature/grind-by-weight`, Repository-ZIP vom
23.10.2026, 16:40 Uhr. Erst nach Prüfung auf Hardware committen.

## Umfang

- `grinder_relay.cpp` ist die einzige Stelle für GPIO16-Schreibzugriffe.
- `grinderRelayInitOff()` setzt die Ausgangslasche zunächst auf HIGH, erst
  dann den Pin auf OUTPUT, anschließend erneut HIGH.
- `grinderRelayForceOff()` führt ein sofortiges HIGH/OFF aus, auch aus den
  asynchronen WebUI-/OTA-Abbruchpfaden.
- `grinderRelayApplyGuard(grinderOutputGuard)` nimmt die Entscheidung der
  getrennten Schutzschicht entgegen und schreibt dennoch ausschließlich
  HIGH/OFF. Der bisherige Diagnosepfad (`wouldPermit`) bleibt erhalten.
- `GRINDER_SSR_PHYSICAL_ENABLE=0` ist in `platformio.ini` festgelegt.
  Ein Wert ungleich 0 führt ausdrücklich zum **Buildfehler**. Es existiert
  weiterhin kein LOW-Schreibpfad für GPIO16.

Damit können wir die zukünftige Hardwaregrenze integrieren und die
Abschaltwege testen, ohne die Mühle oder das SSR aktiv zu schalten.

## Sicherheitsgrenze

**Nicht freigegeben für 230-V-Betrieb.** Die Schnittstelle ist kein
unabhängiger Hardware-Watchdog. Wenn der ESP in einem späteren Stand
GPIO16 auf LOW gesetzt hat und die CPU oder `loop()` anschließend hängt,
kann diese Software allein den Ausgang nicht zuverlässig zurücksetzen.
Nach Reset kann ein GPIO vor `setup()` hochohmig sein; `digitalWrite(HIGH)`
im Setup kann diesen Zeitraum nicht absichern.

Vor einer physischen Freigabe müssen wir zwingend klären und getrennt
testen:

1. Exakte Verdrahtung und Eingangsschaltung des 5-V-LOW-Trigger-SSR
   einschließlich Stromaufnahme, Pegeln und galvanischer Trennung.
2. Ausgangszustand beim Einschalten, Reset, Brownout, Programmabsturz,
   abgezogenem USB und fehlendem ESP – bei **unversorgter Netzlast**.
3. Einen **unabhängig vom Hauptloop und ESP-Zustand wirkenden**
   Abschaltweg, der auch ein dauerhaft auf LOW festhängendes GPIO nicht
   als Dauerlauf der Mühle weitergibt.
4. Boot-, WebUI-/OTA- und Fehlerpfade einschließlich zeitlicher Messung.
5. Klare Freigabe zu einem späteren, separat geprüften LOW-Ausgangs-Patch.

ESP32-S3-GPIO16 ist kein Strapping-Pin, aber das ist **keine** Zusicherung
eines sicheren HIGH-Pegels während Reset und Boot. Auf keinen Fall
Netzspannung nur aufgrund eines erfolgreichen Software-Dry-Runs anschließen.

## Host-Test (ohne PlatformIO)

Im Projektverzeichnis unter Linux/macOS oder in einer Shell mit `g++`:

```sh
g++ -std=c++11 -Wall -Wextra -Werror -Isrc -Itest_host/fakes \
  src/grinder_relay.cpp src/grinder_output_guard.cpp \
  test_host/grinder_relay_b4c2a_test.cpp \
  -o grinder_relay_b4c2a_test
./grinder_relay_b4c2a_test
```

Dieser Test protokolliert `digitalWrite` und `pinMode` ohne Hardware.
Er prüft die HIGH-vor-OUTPUT-Initialisierung, ausbleibendes LOW trotz
Guard-Freigabe, OTA-/WebUI-Abbruch und Nachmahlen.

## USB-Test (Mühle weiterhin vollständig vom 230-V-Netz getrennt)

1. Build und Upload ausführen. Es dürfen keine neuen GPIO-LOW-Logs auftreten.
2. GPIO16 über einen geeigneten Messaufbau beobachten (keine Netzlast).
   Im laufenden Programm muss HIGH/OFF anliegen.
3. Simulierten Mahlzyklus und manuelles Nachmahlen testen: In der Diagnose
   tritt `wouldPermit=1` auf, GPIO16 bleibt dennoch HIGH.
4. WebUI-Tara während des Laufs: Sicherheitsabbruch, GPIO16 HIGH.
5. `/update/reboot` während der Simulation: `inhibit=5`, danach Neustart;
   GPIO16 bleibt im laufenden Programm HIGH. Den Boot-Zeitraum bei Bedarf
   gesondert mit geeignetem Messgerät/Oszilloskop beurteilen.

Die WebUI-Nacharbeit bleibt eine separate Aufgabe.
