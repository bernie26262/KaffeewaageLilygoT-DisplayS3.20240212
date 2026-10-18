# B4a.3a – Vorbereitung Encoder, Sollgewichte und Bedien-Sperren

Ausgangsstand: `c5bc43f` auf `feature/grind-by-weight` (B4a.2 getestet).

## Umfang (noch KEINE Mahlsimulation)

- `GrinderEncoderButton` ist eine nicht blockierende, hardwarefreie
  Erkennung von kurzem Klick und langem Druck (25 ms Entprellung,
  1000 ms Langdruck). `main.cpp` liest GPIO18 als active-LOW-Taster.
- **Der Langdruck ist in B4a.3a absichtlich deaktiviert**:
  `kEnableGrinderSimulationLongPress = false`. Der kurze Klick ruft weiterhin
  `rotary_onButtonClick()` und damit das vorhandene Menü auf. Es gibt keinen
  Aufruf von `grinderCycleGate.requestStart()` / `requestCancel()`.
- `GrinderTargetSync` gleicht die gewünschten `setWeightST[]`-Werte
  (HMI Seite 4 oder WebUI) mit den aktuellen Profilen im Detektor ab.
  Eine Änderung im bereits vorbereiteten Zustand wird **nicht** sofort
  in `setProfile()` übernommen, sondern erst in `WaitVessel`, wenn noch
  kein Mahlzyklus konsumiert wurde. Bis dahin ist
  `grinderTargetSyncPending = true` (Start in B4a.3b sperren).
- Nach Profil-Neusynchronisierung wird bei Autodetect AUS die manuelle
  Profilauswahl wiederhergestellt; `setProfile()` setzt die aktuelle Auswahl
  im Detektor zurück.
- Sobald ein späterer Simulationszyklus begonnen wurde
  (`grinderCycleGate.cycleConsumed()`), sind User-Tara und rechtsseitiges
  Skip/Save gesperrt; die von der Profilerkennung selbst angeforderte
  Abhebe-Tara bleibt möglich. Dies gilt auch für WebUI-Tara über `doTara()`.
- SSR bleibt wie zuvor nur in `grinderRelayInitOff()` auf HIGH/AUS.
  Die neue Logik greift nicht auf GPIO16 zu.

## Grenzen / für B4a.3b und B4a.4 ausdrücklich offen

- Noch keine langdruckbasierte Bedienaktion, Zustandsanzeige oder
  Prüfgewichts-Mahlsimulation; keine Motoransteuerung.
- B4a.3b MUSS bei einem Start explizit prüfen, dass die Sollgewichte
  synchron sind (`!grinderTargetSyncPending`) und frische HX711-Werte vorliegen.
- B4a.3b muss die automatische Legacy-Dosenstatistik, weitere
  WebUI-Befehle und Seitenwechsel während eines laufenden Zyklus untersuchen.
- B4a.4 prüft negativen Gewichtssprung und vollständige Abbruch-Sicherheit.
- Bereits vorbereitetes Gefäß mit nachträglich geändertem Sollgewicht:
  erst entnehmen und leer-/nullgestellt neu vorbereiten. Vorher kein Start.

## Tests

- `test_host/grinder_encoder_button_b4a3a_test.cpp`: kurzer Klick,
  Prellen, Langdruck, Unterdrückung eines zweiten Klicks beim Loslassen,
  Langdruck deaktiviert, gedrückte Taste beim Booten, millis()-Überlauf.
- `test_host/grinder_target_sync_b4a3a_test.cpp`: Zieländerungen in
  ReadyAuto dürfen die Erkennung nicht zurücksetzen. Nach Verlassen der
  Bereitschaft werden die Ziele synchronisiert; manuelle Auswahl bleibt
  wiederherstellbar.
- Bestehende Host-Tests bleiben unverändert.
- Zunächst `git apply --check`, `git apply`, `git diff --check`, PIO-Build;
  anschließend USB-only-Hardwaretest der vorhandenen HMI-Bedienung und
  der automatischen/Skip/manuellen Tara; SSR bleibt AUS.
- Erst nach Hardware-Freigabe Commit und Push.
