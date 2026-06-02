# Storage / Preferences

## Ziel

Alle dauerhaften Werte werden zentral ueber `src/coffee_storage.cpp` verwaltet. Dadurch bleibt `main.cpp` frei von direkter NVS-/Preferences-Detailarbeit.

## Namespace

Der Preferences-Namespace lautet:

```text
savedValues
```

## Wichtige gespeicherte Werte

| Key | Bedeutung |
|---|---|
| `savedWeightST` | gespeicherte Siebtraeger-Gewichte |
| `savedWeightTri` | gespeichertes Trichtergewicht |
| `savedWeightGef` | gespeicherte Gefaess-Gewichte |
| `svdSetWeightST` | Zielgewichte je Siebtraeger |
| `savedCalFact` | Kalibrierfaktor |
| `savedCalWeight` | Kalibriergewicht |
| `savedSelST` | ausgewaehlter Siebtraeger |
| `savedSelGef` | ausgewaehltes Gefaess |
| `savedAutoDetect` | Autodetect ein/aus |
| `grndWghtFrvr` | Mahlgut gesamt |
| `grndWghtCln` | Mahlgut seit Muehlenreinigung |
| `grndWghtKffm` | Mahlgut seit Kaffeemaschinenreinigung |
| `grndWghtFlt` | Mahlgut seit Filterwechsel |
| `shotsFrvr` | Shots gesamt |
| `shotsCln` | Shots seit Muehlenreinigung |
| `shotsKffm` | Shots seit Kaffeemaschinenreinigung |
| `shotsFlt` | Shots seit Filterwechsel |
| `lstMhlRngng` | letzter Zeitpunkt Muehlenreinigung |
| `lstKffmRngng` | letzter Zeitpunkt Kaffeemaschinenreinigung |
| `lstFltwchsl` | letzter Zeitpunkt Filterwechsel |
| `nvsInitialised` | allgemeine Erstinitialisierung |
| `statsV2Init` | Nachmigration neuer Statistikwerte |
| `mntMachSec` | Wartungsintervall Kaffeemaschine in Sekunden |
| `mntGrndSec` | Wartungsintervall Muehle in Sekunden |
| `mntFiltSec` | Wartungsintervall Filter in Sekunden |
| `mntMachEn` | Wartung Kaffeemaschine aktiv/inaktiv |
| `mntGrndEn` | Wartung Muehle aktiv/inaktiv |
| `mntFiltEn` | Wartung Filter aktiv/inaktiv |

## Initialisierung

`coffeeStorageLoadOrInit(...)` prueft, ob `nvsInitialised` vorhanden ist.

Falls nicht, werden die Default-Werte aus dem laufenden Programmzustand in NVS geschrieben und danach wieder gelesen.

## Kompatibilitaet

Einige Werte werden bewusst kompatibel gelesen, z.B. Mahlgut-Gesamtwerte:

```cpp
groundWeightForever = preferences.getFloat("grndWghtFrvr", preferences.getULong("grndWghtFrvr", 0));
```

Das ist eine Bruecke fuer aeltere Installationen, bei denen einzelne Werte frueher als `ULong` statt als `Float` gespeichert wurden. Diese Kompatibilitaetslogik nicht ohne Migrationsplan entfernen.

## Verhalten bei deaktivierter Wartung

Die Wartungs-Enabled-Flags deaktivieren nur Warnungen und das Hochzaehlen der zugehoerigen Wartungszaehler. Die bereits gespeicherten Shots-/Mahlgut-Werte bleiben erhalten.

Bei `Save Dose` gilt:

- Gesamtwerte laufen immer weiter.
- Zaehler seit Kaffeemaschinenreinigung laufen nur, wenn `mntMachEn=true`.
- Zaehler seit Muehlenreinigung laufen nur, wenn `mntGrndEn=true`.
- Zaehler seit Filterwechsel laufen nur, wenn `mntFiltEn=true`.
- Nach Reaktivierung laufen die jeweiligen Werte ab dem vorher gespeicherten Stand weiter.

## Regeln fuer neue Speicherwerte

- keine direkten `preferences.begin/put/get/end`-Zugriffe in `main.cpp`
- neue Speicherfunktionen in `coffee_storage.cpp` ergaenzen
- Deklarationen in `coffee_storage.h` ergaenzen
- bestehende Keys nicht umbenennen, ohne Migration vorzusehen
- nach Storage-Aenderungen immer testen, ob bestehende Werte erhalten bleiben

## Kontrollsuche

`main.cpp` sollte keine direkten Preferences-Zugriffe enthalten:

```powershell
Select-String -Path src\main.cpp -Pattern "preferences\.begin|preferences\.put|preferences\.get|preferences\.end" -CaseSensitive:$false
```

Erwartung:

```text
keine Treffer
```
