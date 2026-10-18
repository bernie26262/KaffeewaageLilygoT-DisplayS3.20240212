# Phase B3e – Optionaler Trichter: Skip-Taste (SSR bleibt AUS)

Ausgangsstand: `feature/grind-by-weight`, Commit `9afb662` (B3d auf Hardware getestet).

## Bedienung

- Bei **Autodetect EIN**, Waagenseite (`pageID=0`) und Zustand `WaitFunnel`
  (bekannter Siebträger erkannt) ersetzt **Skip** die Beschriftung des rechten
  Tasters. Links bleibt **Tara** unverändert.
- Rechter Tastendruck nimmt **nur** eine Skip-Anforderung entgegen und darf
  nicht zusätzlich als `Save` zählen. Skip wird nur bei **frischer, stabiler
  Messung** und einem Gewicht passend zum bereits erkannten Siebträger
  angenommen. Ein abgewiesener Versuch lässt die Waage in `WaitFunnel`.
- Skip fordert `AutoNoFunnel` als eigenen einmaligen Tara-Auftrag an. Die
  B3d-Integration verwendet weiterhin `doTara()` und bestätigt das tatsächliche
  HX711-Ergebnis. Anschließend wird erst bei einer **neuen, stabilen
  Nullmessung** `ReadyNoFunnel` / `ready=1` erreicht.
- Erkanntes Siebträgerprofil und Sollgewicht bleiben erhalten. Es wird kein
  Profil und kein Autodetect-Zustand in NVS überschrieben.
- Bei Abheben: Bereitschaft sofort zurücknehmen, `AfterRemoval`-Tara,
  anschließend `WaitVessel ready=0`. Bei Tara-Fehler oder Zeitüberschreitung
  keine Bereitschaft (`Fault`).
- Beim normalen Auflegen des Trichters bzw. der vollständigen Kombination
  bleibt der B3d-Weg `AutoCombination` -> `ReadyAuto` erhalten.
- **Wichtig:** Skip ist die bewusste Entscheidung *ohne Trichter*. Den Trichter
  nicht nach Skip auf die bereits tarierte Kombination setzen; für einen
  Wechsel zuerst den Siebträger abheben und die Erkennung neu durchlaufen.

Links **Tara** ist weiterhin eine manuelle Nullstellung und erzeugt bei
Autodetect EIN keine Mahlbereitschaft. Zum Überspringen ausschließlich rechts
**Skip** drücken.

## Sicherheit

`ReadyNoFunnel` bedeutet ausschließlich, dass die Waagenvorbereitung fertig ist.
Die reale Mühlen-/SSR-Ansteuerung ist weiterhin **nicht angebunden** und GPIO16
wird von B3e nicht eingeschaltet. Ein SW-Log `SSR OFF` beweist keinen sicheren
Relaiszustand während Reset/Boot oder unter Netzversorgung.

## Tests

Hosttest: `g++ -std=c++11 -Wall -Wextra -Werror -Isrc src/grinder_profile_detector.cpp test_host/grinder_skip_funnel_b3e_test.cpp -o grinder_skip_funnel_b3e_test`.

Hardware (nur USB, Steuergehäuse ohne Netzspannung, Mühle abgesteckt):

1. Siebträger einzeln auflegen. Meldung **Trichter auflegen**, rechts **Skip**.
2. **Skip** drücken. Log `Skip funnel accepted`, `Auto-tare requested: skip funnel`,
   danach `WaitPostTare` und `ReadyNoFunnel ready=1`. Gewichtsnullpunkt prüfen.
3. Ein bekanntes Prüfgewicht (etwa 5–10 g) auflegen: kein neuer Tara-Auftrag.
4. Siebträger abheben: `CheckingRemoval`, einmalige Removal-Tara,
   `WaitVessel ready=0`. Rechts wieder normaler Buttonzustand.
5. Anschließend den Weg **mit** Trichter nochmals prüfen (`ReadyAuto`).
6. Links **Tara** beim Zustand `WaitFunnel` kontrollieren: keine falsche
   `ReadyNoFunnel`-Meldung. Danach für einen erneuten Skip-Test Siebträger
   abheben und neu auflegen.
7. Keine Motorfreigabe. Bei Reset, Tara-Fehler oder unklarem Verhalten abbrechen.

B3e erst nach Build **und** Hardwaretest committen.
