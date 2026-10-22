# B4a.3b – Erste logische Mahlsimulation (USB-only, OHNE SSR-Freigabe)

Ausgangsstand: `654657c` (`feature/grind-by-weight`, B4a.3a). Die neue
Integration wird nur mit vom Netz getrennter Mühle und USB-Versorgung getestet.

## Bedienung, zunächst **nur für die Simulation**

- Hauptseite `pageID=0`, Display an, Single-Dose-Modus, kein Shot:
  - **Encoder > 1 Sekunde halten:** Einmaliger logischer Mahlstart, nur wenn
    Gate `requestStart()` zulässt (bereites Profil, frischer stabiler Nullwert,
    kein offener Sollgewichtsabgleich, kein Wizard/Initialisierung).
  - Bei fehlender Bereitschaft wird der lange Druck **ohne** HMI-Menüaktion
    abgewiesen; beim Loslassen erfolgt kein zusätzlicher kurzer Klick.
  - **Encoder drehen während `Grinding`:** Keine Menünavigation.
  - **Kurzer Encoder-Klick während `Grinding`:** Logischer Abbruch;
    anschließend `Settling` und `Complete`. Beim Klick wird kein Menüpunkt
    aktiviert.
  - **Encoder > 1 Sekunde nach Fault:** Nur auf leerer, korrekt
    zurückgesetzter Waage Fault quittieren. Keine automatische Wiederaufnahme.
  - Kurze Klicks in `Settling` werden ignoriert. Danach bleiben normale
    kurze Klicks Menüaktionen.
- Auf der Hauptseite zeigt der Status rechts in Zeile 3 `SIM MAHLT`,
  `SIM STOPP`, `SIM FERTIG` oder `SIM FEHLER`. Aktuelles Gewicht und Sollgewicht
  bleiben in den vorhandenen HMI-Feldern. Kein neues Menü oder WebUI-Element.

## Sicherheitsbedingungen

- `GrinderCycleGate` arbeitet ausschließlich logisch; es existiert **keine**
  Verbindung von `logicRunning()`/`relayRequested()` zu `GrinderRelay`.
- GPIO16 wird nur über das bisherige `grinderRelayInitOff()` auf HIGH/AUS
  initialisiert. Auch während `Grinding` ist eine Logmeldung
  `logicMotorRequest=1 (SSR OFF)` **keine** physische SSR-Ansteuerung.
- Startschutz: `ReadyAuto`, `ReadyNoFunnel` oder `ReadyManual`, Sollgewicht
  5–25 g, frisches stabiles Gewicht innerhalb ±1 g, kein bereits benutzter
  Mahlzyklus, kein ausstehender Sollgewichtsabgleich und keine veränderte
  HMI-Profilauswahl.
- Sensorwerte unter −3 g während `Grinding`, `ManualTopup` oder `Settling`
  verriegeln **sofort** den Fehlerzustand, auch wenn die Profilerkennung
  die Bereitschaft erst beim nächsten Schritt entzieht. Kleine Drifts wie
  −1,4 g dürfen diesen zusätzlichen Grenzwert nicht auslösen.
- Ein nicht endlicher Gewichtswert in diesen Zuständen führt ebenfalls zu
  `Fault`. Die bereits vorhandene Profilmismatch-, Stale-Sample- und
  maximale Laufzeitüberwachung bleibt erhalten.
- Wechsel von Hauptseite, Modus, manuell ausgewähltem Profil, Web-Wizard,
  Shot oder ausstehender Sollgewicht-Synchronisierung während eines aktiven
  Zyklus verriegelt `Fault`.
- Tara, Skip und Save sind nach Zyklusstart gesperrt. Die von der
  Profilerkennung angeforderte Abhebe-Tara darf weiter erfolgen.
- Für einen erneuten Zyklus muss die Waage vollständig zurückgesetzt und
  neu vorbereitet werden. `Fault` erfordert zusätzlich eine explizite
  Quittierung im leeren Zustand.

## Durchführung des Hardwaretests

1. Steuergehäuse vom Stromnetz trennen, Mühle abstecken und ESP32 nur per USB
   versorgen. Im Serial Monitor sicherstellen, dass kein physischer Motor
   geschaltet wird. `motorRequest=1` ist in dieser Phase rein logisch.
2. Leere Waage: langer Encoder-Druck darf weder Mahlstart noch Menüaktion
   erzeugen. Kurze Klicks müssen normal navigieren.
3. Im Autodetect-Modus Siebträger + Trichter vorbereiten, auf bestätigte
   `ReadyAuto`-Nullstellung warten. Mit langem Druck logische Simulation
   starten. Der TFT muss `SIM MAHLT` zeigen.
4. Mit Prüfgewichten innerhalb von **30 Sekunden** in kleinen Schritten bis
   zur Stoppschwelle (Sollgewicht minus 0,25 g) auflasten. Der Status soll
   `SIM STOPP`, danach `SIM FERTIG` anzeigen. Keine Save- oder Tara-Aktion.
5. Prüfen, dass ein weiterer langer Druck bei noch aufgelegtem Siebträger
   keinen Neustart auslöst. Nach vollständigem Abheben, automatischem
   Zurücktaren und erneutem Auflegen darf ein neuer Zyklus beginnen.
6. Mit neuer Vorbereitung kurzen Klick während `SIM MAHLT` prüfen:
   Stopp ohne Menünavigation. Danach Trichter-Skip und Autodetect AUS mit
   manueller Tara ebenfalls mit logischem Start testen.
7. Gezielter Fehlerfall nur USB-only: laufende Simulation durch Abheben
   abbrechen. Erwarten: `SIM FEHLER`, kein logischer Motorwunsch. Nach
   Rückkehr zu `WaitVessel` per langem Druck quittieren.
8. Seitenwechsel, ungültiger Sensorwert, Ausfall von HX711 und Timeouts
   werden zusätzlich über Host-Tests geprüft; B4a.4 vertieft das.

## Prüfung und Grenzen

- Host-Tests unter `test_host/`, einschließlich
  `grinder_simulation_b4a3b_test.cpp`; keine Hardware- oder Relay-Abhängigkeit.
- Änderungen beschränken sich auf `main.cpp`, Gate und Tests/Dokumentation.
- Kein elektrischer GPIO-Test oder PlatformIO-Build in der Entwicklungsumgebung
  erfolgt; diese Tests müssen auf Windows und danach auf echter USB-Hardware
  durchgeführt werden.
- Langdruckbelegung ist **temporär** und keine Entscheidung für das
  spätere Produktions-HMI.
- WebUI-Nacharbeit ist ein eigener, ausstehender Arbeitsblock.
