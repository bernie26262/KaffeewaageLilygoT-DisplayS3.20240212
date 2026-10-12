# Grind-by-Weight – Phase B2 (Profil-/Trichtererkennung, ohne Hardwarekopplung)

## Ausgangspunkt

Die Altfirmware `Autodetect()` verwendet vier Profile mit `weightST`,
`weightTrichter` und `setWeightST`, erkennt nach einer Laständerung > 30 g und
1.500 ms Wartezeit das Siebträgergewicht (±2 g) oder das Summengewicht von
Siebträger und Trichter. Bei vollständig erkannter Kombination wird tariert.
Die Meldung „Trichter auflegen“ ist ein Hinweis, keine generelle Mahlfreigabe.

Die **aktuelle** Legacy-Basis enthält diese drei Profile-Arrays bereits und
speichert sie über `coffee_storage`. B2 führt **keinen neuen NVS-Schlüssel**
und kein zweites Datenmodell ein. In B3 sollen die vorhandenen Arrays an
`GrinderProfileDetector::setProfile()` übergeben werden.

## Schnittstelle und Ablauf

- `setProfile(i, {weightST[i], weightTrichter[i], setWeightST[i]})` lädt bis zu
  vier Profile. Gültige Sollgewichte: 5–25 g.
- `onWeightSample(g, stable, millis)` verarbeitet Messungen ohne Verzögerungs-
  schleifen. `stable` muss in B3 aus einer **echten** Stabilitätserkennung kommen,
  nicht aus dem vorläufigen `appState.weight.stable = true`.
- Auto-Modus: Siebträger allein -> `WaitFunnel`/Hinweis „Trichter auflegen“,
  Gesamtgewicht -> `AwaitTare`. Auch das direkt aufgelegte Gesamtgewicht wird
  erkannt. Mehrdeutige Profile führen **nicht** zu automatischer Tarierung.
- `takeTareRequest()` liefert einmalig `AutoCombination` oder nach manuellem
  Auslösen `Manual` bzw. nach Abheben `AfterRemoval`. Erst nach **erfolgreicher
  HX711-Tara** darf `onTareResult(true, millis)` aufgerufen werden. Danach
  muss ein frischer stabiler Nullwert (±2 g) eintreffen, erst dann wird
  `ReadyAuto` bzw. `ReadyManual` erreicht.
- Fremdes Gefäß: Autodetect ausschalten, Profil/Sollgewicht wählen,
  `requestManualTare()` nach stabilem Messwert auslösen. Der Trichter ist keine
  zwingende Voraussetzung für manuelle Bedienung.
- Nach erfolgreicher Tara wird die alte Bruttomessung verworfen. Negative
  Gewichtsänderung >30 g entzieht bei Abheben sofort die Bereitschaft. Nach
  stabil bestätigtem Abheben fordert der Detektor eine erneute Null-Tara an.
- Fehlende neue Messungen (> 1 s), ungültige Werte oder fehlgeschlagene bzw.
  überfällige Tara oder ausbleibende Null-Bestätigung (je 5 s) lassen die
  Bereitschaft erlöschen. Diese Zeiten sind
  **vorläufige** Integrationsparameter.

## Wichtige Grenzen dieser Phase

B2 ist **noch nicht** mit `main.cpp`, HX711, HMI, WebUI oder `GrinderController`
verbunden. Es enthält keinerlei GPIO- oder Relaisausgabe. **Das SSR bleibt AUS.**
`readyToGrind()` ist lediglich ein Logikstatus, kein Motorbefehl.

Die heutige ESP32-Lastzell-API und die Stabilitätsberechnung, Bedienbuttons,
Seitensperre bei laufendem Mahlen, sowie NVS-Wertevalidierung werden erst in B3
zusammengeführt. Vor einer SSR-Freigabe zusätzlich Hardware-AUS-Tests bei
regulärer 5-V-Versorgung und Boot/Reset sowie Abschaltprüfungen durchführen.

## Host-Tests (g++ unter Windows, z. B. MSYS2)

```sh
g++ -std=c++11 -Wall -Wextra -Werror -Isrc src/grinder_profile_detector.cpp test_host/grinder_profile_detector_test.cpp -o grinder_profile_detector_test
./grinder_profile_detector_test
```
