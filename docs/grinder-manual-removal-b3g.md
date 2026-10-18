# Phase B3g – Automatische Nullstellung nach manuellem Abheben (SSR AUS)

Aufbauend auf **B3f**; B3e/B3f bleiben noch im Worktree, falls nicht committed.

## Hintergrund / Fehlerursache

Bei `Autodetect = AUS` war `ReadyManual` nach manuellem Auflegen und Tara
korrekt, und das Abheben ging korrekt auf `CheckingRemoval` (`ready=0`).
Der Detektor erzeugte anschliessend `AfterRemoval`, aber `main.cpp`
verweigerte die automatische Tara pauschal wegen `autoDetect == 0`.
Die anschliessende Fehlerquittierung setzte `Fault`.
Eine weitere Benutzertara auf der nun leeren Waage konnte `ReadyManual`
falsch herstellen.

## Neue Regeln

1. **Automatische Rueck-Tara nach Abheben** ist in `ReadyManual`,
   `ReadyAuto` und `ReadyNoFunnel` zulaessig, unabhaengig von Autodetect.
   Wie zuvor: erst sicher bestaetigtes Abheben, dann *einmalige* HX711-Tara,
   anschliessend frischer stabiler Nullwert. `ready=0` bleibt nach Abheben
   und auch nach der Rueck-Tara bestehen.
2. **Tara ohne aufliegendes Gefaess:** Die Tara selbst ist erlaubt und
   korrigiert 0 g; es entsteht aber **kein** `ReadyManual`. Fuer eine neue
   manuelle Bereitschaft muss zuvor ein stabiles, deutlich positives
   Bruttogewicht (mindestens die vorhandene `placeThresholdG` von 30 g)
   erfasst worden sein und ein manuelles Profil gewaehlt sein.
3. **Manuelle Nach-Tara bei bereits `ReadyManual`:** Eine kleine stabile
   Nullpunktkorrektur bis +/-1,0 g behaelt das Profil bei; `ready` wird
   waehrend der Tara gesperrt und erst nach stabiler Null bestaetigt.
   Mit einer aufgelegten Dosis von ueber 1 g wird Tara blockiert.
4. **TFT/WebUI:** Beide verwenden weiterhin dieselbe Tara-Freigabe,
   jetzt auch bei `ReadyManual` und waehrend der Abhebe-/Rueck-Tara-Phase.
   Eine Fehlertara darf nicht automatisch wieder Bereitschaft herstellen.
5. `AutoCombination` und `AutoNoFunnel` verlangen **weiterhin**
   `Autodetect = EIN`; nur `AfterRemoval` ist unabhaengig.

Diese Regeln erfordern einen bestaetigten Lastwechsel. Eine schlichte
HX711-Tara kann nicht erkennen, ob die Waegeplattform leer ist, wenn sich
bereits ein unbekannter Taraoffset auf der Waage befindet. Im normalen
Ablauf wird dieser Offset durch die Rueck-Tara aufgehoben.

## Tests (USB-only, Netzspannung aus, Muehle abgesteckt)

1. Autodetect AUS, Profil waehlen, leere Waage Tara -> 0 g, `ready=0`.
2. Siebtraeger (~450 g) auflegen, Tara -> `ReadyManual ready=1`.
3. Unter 1 g Drift simulieren, Re-Tara -> erneut `ReadyManual`.
4. Groesseres Pruefgewicht hinzufuegen, Tara muss blockiert sein.
5. Siebtraeger vollstaendig abheben -> `CheckingRemoval ready=0`,
   `Auto-tare requested: removal`, `Tare complete`,
   `WaitVessel ready=0`, etwa 0 g ohne manuellen Tastendruck.
6. Optional erneut Tara auf leerer Waage -> weiterhin `ready=0`.
7. Autodetect EIN und die bisher getesteten Skip- und Trichterablaeufe
   nochmals gegenpruefen.

Kein GPIO16-Einschaltpfad. Build und Hardwaretest sind noch offen.
