# Grind-by-Weight – Phase B1 (logischer Controller, ohne SSR-Freigabe)

Der neue `GrinderController` ist **noch nicht mit GPIO16 oder dem HMI verbunden**.
`grinderRelayInitOff()` bleibt die einzige verwendete SSR-Funktion. Auch ein
Controller-Status `relayRequested()==true` kann in Phase B1 keinen Motor starten.

## Abgleich mit dem Altprojekt

- 4 Siebtraeger-Profile und gespeicherte Sollgewichte: spaeter in HMI/NVS integrieren.
- Sollgewichtsbereich 5..25 g wie im alten Muehlenprojekt.
- Normales Mahlen bei Gewicht unter `Soll - 0,5 g`, automatischer Stopp bei
  `Ist >= Soll - Nachlauf`, Nachlaufstartwert 0,25 g.
- Manuelles Nachmahlen bei Gewicht nahe dem Soll: nur waehrend gedrueckter Taste;
  im Altcode konnte die Freigabe an Istgewicht und HMI-Seite haengen.
- Endgewichtspruefung nach 450 ms und Nachlauf-Anpassung 0..2 g.

## Sicherheit / vorlaeufige Parameter

- Kein automatischer Wiederanlauf nach Reset oder Fehler.
- Keine Bewegung aus WLAN, WebUI oder OTA. Hardwarefreigabe fehlt absichtlich.
- Manuelles Nachmahlen endet **immer** beim Loslassen.
- 30 s maximale ununterbrochene Laufzeit; 1 s ohne neuen gueltigen HX711-Wert:
  Fehler und sofortige OFF-Anforderung. Werte sind **vorlaeufig**.
- Ungueltiger Gewichtswert (NaN/Inf) waehrend des Mahlens: Fehler und OFF.
- Not-Aus per `emergencyStop()` verriegelt den Fehlerstatus bis `clearFault()`;
  neue Messung und erneuter Tastendruck sind danach notwendig.
- UI-Seitenwechsel haben keinen Einfluss auf den Zustand.
- Die spaetere reale Freigabe braucht zusaetzlich eine gepruefte elektrische
  Fail-safe-Schaltung und eine unabhängige SSR-Abschaltstrategie waehrend OTA.

## Lokaler Logiktest (falls g++ installiert ist)

```sh
g++ -std=c++11 -Wall -Wextra -Werror -Isrc src/grinder_controller.cpp test_host/grinder_controller_test.cpp -o grinder_controller_test
./grinder_controller_test
```

Unter PlatformIO wird die neue `.cpp` zwar kompiliert, der Controller jedoch
noch nicht instanziiert oder mit der SSR-Ausgabe verbunden.
