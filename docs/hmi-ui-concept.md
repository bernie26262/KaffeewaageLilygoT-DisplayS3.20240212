# HMI-/WebUI-Bedienkonzept

## Ziel

Die WebUI der Kaffeewaage soll mittelfristig als Vorlage fuer ein lokales Touch-HMI dienen. Zielhardware fuer eine spaetere HMI-Variante ist ein LilyGO T4 S3 bzw. ein vergleichbares ESP32-S3-Board mit Touch-Display und ca. 450 x 600 px Aufloesung.

Die WebUI soll deshalb nicht wie eine klassische Webseite aufgebaut sein, sondern wie eine eingebettete Bedienoberflaeche:

- wenige Hauptbereiche
- klare Touch-Flaechen
- dunkles Farbschema
- gruene Akzentfarbe
- Bottom-Navigation
- keine ueberladenen Dashboard-Seiten
- moeglichst wenig Scrollen auf den wichtigsten Seiten

## Hauptnavigation

Die Hauptnavigation besteht aus vier Bereichen:

```text
Waage | Stoppuhr | Daten | Einstellungen
```

Diese Bereiche sind in der WebUI aktuell Tabs innerhalb der Hauptseite `/`. Sie sind keine eigenen URLs.

Fuer das spaetere HMI sollen diese Bereiche als echte Screens umgesetzt werden:

```text
ScaleScreen
TimerScreen
DataScreen
SettingsScreen
```

## Waage

Die Waage-Seite ist die wichtigste Bedienseite und soll nicht scrollen muessen.

Sie enthaelt nur die direkt benoetigten Bedienelemente:

- grosses Gewicht
- Sollgewicht
- Tara
- Save Dose
- Autodetect
- Siebtraegerauswahl

Nicht auf die Hauptseite gehoeren:

- erkannte Gefaess-Details
- Log-Ausgaben
- Systemdaten
- OTA
- Wartungsdetails

Diese Informationen liegen in Daten oder Einstellungen.

## Stoppuhr

Die Stoppuhr soll ebenfalls ohne Scrollen bedienbar sein.

Inhalte:

- grosse Zeitanzeige
- Start/Stopp
- Reset

Die Button-Optik soll dem restlichen HMI folgen: dunkle Buttons mit gruenem Rand. Vollton- oder Warnfarben bleiben Sonderaktionen vorbehalten.

## Daten

Die Daten-Seite darf moderat scrollen, weil dort eher gelesen als schnell bedient wird.

Inhalte:

- Gesamt-Shots
- Gesamt-Mahlgut
- Uptime
- Wartungs-/Statistikwerte
- ggf. spaetere Diagnose- oder Verlaufwerte

Zeitangaben sollen einheitlich formatiert werden:

```text
00:00:00
1 Tag, 00:00:00
2 Tage, 00:00:00
```

## Einstellungen

Die Einstellungen sind in Unterbereiche gegliedert:

```text
Wartung
Waage & Gefaesse
System / OTA
```

In der WebUI sind diese Unterbereiche aktuell als zweite Tab-/Segment-Ebene innerhalb von Einstellungen umgesetzt.

Fuer das spaetere HMI sollen sie moeglichst nicht als lange scrollende Seite erscheinen, sondern als eigene Unterseiten oder Screens:

```text
SettingsMaintenanceScreen
SettingsScaleVesselsScreen
SettingsSystemScreen
```

## Umgang mit Scrollen

Scrollen soll auf dem HMI moeglich, aber nicht die Standardbedienung sein.

Empfohlene Regeln:

- Waage: kein Scrollen
- Stoppuhr: kein Scrollen
- Daten: Scrollen erlaubt
- Einstellungen: besser Unterseiten statt langer Scrollseite
- Bottom-Navigation bleibt sichtbar
- Header bleibt kompakt
- wichtige Aktionen nicht ans Ende einer langen Seite verstecken
- destruktive Aktionen immer mit Bestaetigung

Wenn innerhalb eines Einstellungsbereichs gescrollt werden muss, soll nur der Inhaltsbereich scrollen. Der Bereichsumschalter bzw. die Unterseiten-Navigation sollte sichtbar bleiben oder leicht erreichbar sein.

## Header und Status

Der Header soll kompakt bleiben, damit auf kleinen Displays kein Platz verloren geht.

Aktueller Zieltext:

```text
Single-Dose-Waage
```

Der WebSocket-Status soll kurz bleiben:

```text
Verbinde...
Verbunden
Getrennt
```

Log-Ausgaben gehoeren nicht global auf alle Seiten, sondern in den Bereich System / OTA bzw. spaeter in System / Diagnose.

## Unterschiede zwischen WebUI und HMI

Die WebUI bietet weiterhin Funktionen, die auf dem lokalen HMI nicht benoetigt werden oder dort anders geloest werden:

- OTA-Update ist eine Web-/Computer-Funktion und wird auf dem lokalen HMI nicht benoetigt.
- WLAN-Provisioning ist fuer verkaufbare Geraete wichtiger als fest einkompilierte WLAN-Secrets.
- System-/Debug-Informationen koennen in der WebUI ausfuehrlicher sein als auf dem HMI.

## Gestaltungsgrundsaetze

- keine absoluten/ueberlagernden Touch-Flaechen fuer wichtige Bedienelemente
- normale Layout-Reihenfolge bevorzugen
- grosse, eindeutige Touch-Ziele
- keine zufaelligen Mehrfachausloesungen durch ueberlappende Bereiche
- gruene Akzentfarbe fuer normale Aktionen
- rote Warnfarbe nur fuer destruktive oder gefaehrliche Aktionen
- kleine, testbare UI-Schritte
