# Storage / Preferences

## Ziel

Die Legacy-kompatiblen dauerhaften Werte werden über `src/coffee_storage.cpp` verwaltet. Für die T4-S3 kommen eigene Settings in `src/t4s3_settings.cpp` und WLAN-Profile in `src/coffee_wifi.cpp` hinzu. Die Module verwenden getrennte NVS-Namespaces.

## Namespace

Die für die folgende Legacy-kompatible Tabelle verwendeten Schlüssel liegen unter:

```text
savedValues
```

Weitere produktive T4-S3-Namespaces:

- `t4s3ui`: HMI, Gefäße, Kalibrierung, Wartung und sonstige T4-spezifische Einstellungen (`src/t4s3_settings.cpp`).
- `coffee_wifi`: WLAN-Profile (`p1_ssid`, `p1_pass`, `p2_ssid`, `p2_pass`) und Steuerwerte (`schema`, `pref`, `auto`). Die alten Schlüssel `ssid`, `pass`, `active` bleiben nach der einmaligen Migration für ein mögliches Rollback erhalten.

Weitere Details: `docs/t4s3-persistent-settings.md` und `docs/wifi-provisioning.md`.

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
