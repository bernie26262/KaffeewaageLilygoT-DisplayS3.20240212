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

## Initialisierung

`coffeeStorageLoadOrInit(...)` prueft, ob `nvsInitialised` vorhanden ist.

Falls nicht, werden die Default-Werte aus dem laufenden Programmzustand in NVS geschrieben und danach wieder gelesen.

## Kompatibilitaet

Einige Werte werden bewusst kompatibel gelesen, z.B. Mahlgut-Gesamtwerte:

```cpp
groundWeightForever = preferences.getFloat("grndWghtFrvr", preferences.getULong("grndWghtFrvr", 0));
```

Das ist eine Bruecke fuer aeltere Installationen, bei denen einzelne Werte frueher als `ULong` statt als `Float` gespeichert wurden. Diese Kompatibilitaetslogik nicht ohne Migrationsplan entfernen.

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


## Shot-Verlauf bewusst nicht persistent

Der Shot-Verlauf wird nicht in NVS, SPIFFS oder LittleFS gespeichert. Gaggiuino übernimmt bereits die dauerhafte Shot-Historie; eine zweite Historie auf der Waage wäre redundant.

Die Waage hält ausschließlich:

- den laufenden Shot oder
- den zuletzt abgeschlossenen Shot

im RAM. Der Puffer umfasst maximal 1.200 Samples bei 10 Hz. Ein neuer Shot überschreibt den alten erst beim tatsächlichen Start. Ein Neustart löscht den Verlauf bewusst.

Dadurch entstehen keine zusätzlichen Flash-Schreibvorgänge und kein Verwaltungsaufwand für alte Shots.
