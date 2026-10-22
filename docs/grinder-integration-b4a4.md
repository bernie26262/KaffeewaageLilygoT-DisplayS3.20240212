# B4a.4 – Schutz bei Sensorstillstand und WebUI-Eingriffen

Ausgangsbasis: `1b71bcf` (`feature/grind-by-weight`), B4a.3b ist auf
USB-only-Hardware getestet. Dieser Schritt schaltet **keinen SSR** frei.

## Änderungen

- `GrinderController`: Die HX711-Aktualitätsprüfung gilt nun nicht nur
  während `Grinding`/`ManualTopup`, sondern auch während `Settling`.
  Ein fehlender Messwert über `maxSampleAgeMs` (standardmäßig 1000 ms)
  führt zu `Fault`/`StaleWeight`, auch wenn der nächste Wert erst spät
  eintrifft. Ein `NaN`/unendlicher Wert während `Settling` führt zu
  `Fault`/`InvalidWeight`. Ein normaler Settling-Abschluss mit frischen
  Messwerten bleibt unverändert.
- `grinder_web_safety.h`: Host-testbare Entscheidung über WebUI-Kommandos:
  Während `Grinding`, `ManualTopup` oder `Settling` wird **jedes** WebUI-
  Kommando sofort abgewiesen. Der WebSocket-Callback setzt eine atomare
  Abbruchanforderung; `loop()` verriegelt den Mahlcontroller beim nächsten
  Durchlauf (`Fault`). So wird die Controller-Instanz nicht parallel aus
  dem WebSocket-Task verändert. Passives WebSocket-Gewichtstelemetrie-
  Streaming ist nicht betroffen.
- Nach begonnenem Simulationszyklus wird `CMD_SAVE_DOSE` auch bei
  `Complete` und `Fault` abgewiesen. Der Save-Handler besitzt zusätzlich
  eine eigene Sperre, damit Testgewichte nicht als echte Dosen in die
  Statistik übernommen werden können.
- Bisherige HMI-Tasten/Encoderbelegung, Siebträger-/Trichtererkennung,
  Tara, GPIO16 und SSR-Firmware bleiben unverändert.

## Host-Tests

`test_host/grinder_safety_b4a4_test.cpp` prüft:

- Stale-Sample-Fault im `Settling` bei `tick()` ohne neues Gewicht;
- Stale-Sample-Fault, wenn ein verspäteter Wert direkt geliefert wird;
- nicht endlichen Gewichtswert während `Settling`;
- normales `Settling → Complete` bei frischen HX711-Messwerten;
- WebUI-Kommando-Abbruch während aktiven Zyklus;
- Save-Sperre bei verbrauchtem Zyklus nach Stopp und Fehler;
- freie Nutzung der bisherigen WebUI, solange noch kein Zyklus läuft.

Alle bestehenden Host-Tests sind ebenfalls erneut auszuführen.

## Hardwaretest (USB-only, Netzversorgung und Mühle getrennt)

1. Flashen und Booten. Leere Waage, HMI-Menü und gewöhnliche WebUI-Bedienung
   vor Mahlstart müssen wie bisher funktionieren.
2. `ReadyAuto`: Encoder lang zum Simulationsstart; Prüfgewichte bis zur
   Stoppschwelle auflegen. `SIM STOPP → SIM FERTIG`, kein realer Motor.
3. Neuer Zyklus nach Abheben: noch während `SIM MAHLT` in der WebUI
   **Tara** auslösen. Erwartet: Kommando **abgelehnt**, keinerlei Tara,
   `SIM FEHLER`, `state=5`, `motorRequest=0`, Log
   `[GBW B4a.4] WebUI command rejected: safety abort queued (SSR OFF)`
   und `[GBW B4a.4] WebUI safety abort applied (SSR OFF)`.
   Nach vollständigem Abheben kann der Fehler explizit quittiert werden.
4. Neuer Zyklus starten und ohne zusätzliche Prüfgewichte länger als 30 s
   laufen lassen. Erwartet: `Fault`, `reason=7` (`RuntimeExceeded`),
   `motorRequest=0`. Die HX711-Messwerte kommen weiterhin normal herein.
5. Bei `ReadyNoFunnel` und `ReadyManual` die bisherigen Starts und das
   HMI-Abbruchverhalten einmal kurz prüfen.

Nicht im Betrieb HX711, SSR, Mühle oder USB-Stecker trennen, um Fehler
zu provozieren. Die Sensorausfälle sind rein in Host-Tests simuliert.

## Grenzen

- Die `[GBW ...]`-Meldung `SSR OFF` zeigt weiterhin nur den logischen
  Softwarepfad. Die elektrische Abschaltfähigkeit ist hiermit **nicht**
  verifiziert. GPIO16 wird in B4a.4 weiterhin nur beim Booten auf HIGH
  initialisiert, und es gibt keine Motorfreigabe.
- Eingriffe über OTA/Neustart und physisches SSR-Failsafe gehören vor B4c
  in eine gesonderte Sicherheitsprüfung.
- Die vollständige WebUI-Nacharbeit und der Funktionsabgleich mit der HMI
  bleiben als eigenständige Aufgabe bestehen.
- Erst nach Host-Tests, PlatformIO-Build und bestätigtem Hardwaretest committen.
