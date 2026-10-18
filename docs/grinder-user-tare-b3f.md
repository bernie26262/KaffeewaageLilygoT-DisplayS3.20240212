# Phase B3f – Zustandsabhaengige Tara und Re-Tara (SSR bleibt AUS)

Voraussetzung: B3e (Skip-Funktion) ist bereits angewendet. B3e wurde auf Hardware mit und ohne Trichter erfolgreich getestet. Dieser Patch baut **auf B3e** auf, nicht auf B3d allein.

## Bedienregeln (TFT-Taste links und WebUI-Tara)

| Zustand | Tara | Resultat |
| --- | --- | --- |
| Autodetect EIN, erkannter Siebtraeger wartet auf Trichter (`WaitFunnel`, `CheckingFunnel`) | gesperrt | Mit Trichter automatisch weitermachen oder rechts `Skip` waehlen |
| Autodetect EIN, `ReadyNoFunnel` nach Skip | nur bei frischer, stabiler Messung nahe 0 (max. +/-1 g) | HX711-Re-Tara; `ready=0` waehrenddessen, danach wieder `ReadyNoFunnel` |
| Autodetect EIN, `ReadyAuto` mit Trichter | identisch | HX711-Re-Tara; danach wieder `ReadyAuto` |
| Autodetect EIN, leerer / unbekannter Gegenstand | normale Tara | **keine** automatische Mahlbereitschaft |
| Autodetect AUS, manuelles Profil | Tara wie bisher | `ReadyManual` nach erfolgreicher Tara und bestaetigter stabiler Nullmessung (sofern Profil und Messwert gueltig) |
| Tara laeuft, Abheben wird geprueft oder Waage pausiert | gesperrt | Kein konkurrierender Tara-Auftrag |

Die reine Tara-Taste stellt beim aufliegenden, noch nicht freigegebenen Siebtraeger **keine** Vorbereitung her. Das erledigt ausschliesslich `Skip` oder die vollstaendige Kombination mit Trichter.

Die Re-Tara hat eigene Tare-Kinds (`ReTareAuto`, `ReTareNoFunnel`) und verwendet denselben blockierenden `LoadCell.tare()`-Pfad wie zuvor. Der Zustand bleibt gespeichert, waehrend die Bereitschaft fuer die Dauer der Tara zurueckgenommen wird. Erst eine **neue, stabile Messung bei 0** (bisherige B3d-Toleranz +/-2 g) bestaetigt die Wiederbereitschaft. Bei Fehler, fehlendem Nullwert oder Timeout entsteht kein `ready=1`.

Die separate +/-1-g-Grenze fuer die Ausloesung einer Re-Tara soll verhindern, dass Kaffee im Siebtraeger versehentlich wieder zu null tariert wird. Sie schuetzt aber nicht vor dem Zuruecksetzen sehr kleiner bereits eingefuellter Mengen. Re-Tara ist deshalb ausschliesslich fuer einen bewusst korrigierten Nullpunkt vor dem Mahlen gedacht. Bei zukuenftiger Motoransteuerung wird die Tara ausserdem waehrend aktiven Mahlens gesperrt werden muessen.

## Host-Tests

```sh
g++ -std=c++11 -Wall -Wextra -Werror -Isrc src/grinder_profile_detector.cpp test_host/grinder_retare_b3f_test.cpp -o /tmp/grinder_retare_b3f_test
/tmp/grinder_retare_b3f_test
```

Geprueft: WaitFunnel-Sperre, Skip -> ReadyNoFunnel -> Re-Tara -> ReadyNoFunnel, Bereitschaftspause, Doppelanforderung, Zugabe von 8,5 g verhindert Re-Tara, Abheben und Removal-Tara, Kombination -> ReadyAuto -> Re-Tara, instabile/veraltete Messwerte, Tara-Fehler, Timeout und manuelles ReadyManual bei Autodetect AUS.

## Hardwaretest (nur USB, Netzspannung aus, Muehle abgesteckt)

1. Autodetect EIN, Siebtraeger ohne Trichter: `Trichter auflegen`, links **kein Tara**, rechts **Skip**. Linke Taste und WebUI-Tara duerfen keine Tara ausfuehren.
2. Skip druecken: `ReadyNoFunnel ready=1`. Bei stabilem Nullgewicht links Tara druecken: direkt `AwaitTare`, dann `WaitPostTare`, danach **erneut** `ReadyNoFunnel ready=1` ohne Abheben.
3. Ein Pruefgewicht von 5 bis 10 g hinzufuegen: linke Tara darf nicht ausgeloest werden, Gewicht darf nicht verschwinden; die Re-Tara ist dafuer gesperrt.
4. Abheben: `CheckingRemoval`, Rueck-Tara, `WaitVessel ready=0`.
5. Siebtraeger plus Trichter: `ReadyAuto ready=1`, nahe 0 erneut Tara -> nach bestaetigtem Nullwert wieder `ReadyAuto ready=1`. Danach Abheben und auf Reset achten.
6. Autodetect AUS: manuelles Profil auswaehlen, Tara und kontrollieren, ob `ReadyManual ready=1` folgt.
7. WebUI-Tara in `WaitFunnel` ebenfalls pruefen: sie muss abgewiesen werden, ohne HX711-Tara auszufuehren.

Kein Git-Commit vor gruenem Build und Hardwaretest. **GPIO16 bleibt AUS; B3f enthaelt keinerlei Motorfreigabe.**
