# B4b.2 – Manuelles Nachmahlen, nur logische Simulation

**Basis:** `f3ab94e` (`feature/grind-by-weight`), Quellstand aus dem ZIP
`KaffeewaageESP_GrindByWeight_Modern.20261022_1637.zip` + B4b.1-Patch.
Die separate WebUI-Funktionsangleichung bleibt offen.

## Bedienkonzept

Der Encoder-Langdruck startet wie in B4a den normalen Mahlzyklus, ein
Encoder-Kurzklick bricht die Simulation ab. **Erst nach einem regulären,
zielgewichtsgesteuerten Abschluss** (`SIM FERTIG`) kann rechts am TFT
`NACHM.` erscheinen. Voraussetzung: Tatsächliches Endgewicht ist mindestens
0,10 g und höchstens 0,50 g unter dem Soll, stabil und frisch; das
Profil ist unverändert und die komplette Bereitschaft liegt weiter vor.

Der **rechte TFT-Taster** wird dann zur *gedrückt halten = nachmahlen*
Bedienung. Er ist bisher nach einem konsumierten Zyklus für Legacy-Save
blockiert; wir verwenden ihn ausschließlich im expliziten Nachmahl-Fenster.
Die neue Button-Klasse entprellt nur das Drücken (25 ms). Beim ersten rohen
Loslassen wird **ohne Verzögerung** `onButtonRelease()` aufgerufen.
Ein Taster, der schon gedrückt war, bevor die Bedingungen erfüllt waren,
kann nicht automatisch nachstarten.

Der Controller fordert `ManualTopup` logisch nur während dieses Haltens an.
Bei Erreichen von Sollgewicht minus 0,05 g, vorzeitigem Loslassen,
unerwarteter Kontextänderung oder Zeitüberschreitung endet die logische
Anforderung. Maximale kontinuierliche Nachmahlzeit: 3000 ms, bei
Überschreitung verriegelter Fehler (`RuntimeExceeded`). Eine manuelle
Nachmahlung verändert **nicht** die in B4b.1 je Profil gelernte
Nachlaufkorrektur. Jeder weitere Nachmahlversuch erfordert eine neue
Tasterbetätigung und bleibt auf den Defizitbereich begrenzt.

## Sicherheitsgrenzen

- Keine neue SSR- oder Motorfreigabe: GPIO16 bleibt physisch **AUS**.
- Vollzyklus-Start via rechtem Taster weiterhin ausgeschlossen.
- Keine Freigabe nach Nutzerabbruch, Sensorfehler, Sicherheitsfehler,
  Überdosierung oder vollständiger Entnahme.
- Bestehende B4a.4-WebUI-Guards sperren relevante Befehle auch bei
  `ManualTopup`.
- HMI-Seitenwechsel, Profil-/Zielwechsel, Sensorfehler und Abheben
  bleiben durch bestehende Gate-Prüfungen abgesichert.
- Bei Erreichen der automatischen Gewichtsschwelle kann `ManualTopup`
  bereits beendet sein, obwohl der rechte Taster noch gehalten wird.
  Dies führt nicht zu einem zusätzlichen Fehlerzustand oder Neustart.

## Serielle Diagnose / manueller Hardwaretest

Nur USB-Versorgung, Mühle elektrisch vom Netz getrennt:

1. Siebträger mit Trichter vorbereiten; durch Prüfgewichte die erste
   Simulation normal bei ca. 8,15 g stoppen und nach 450 ms mit stabilem
   Ergebnis **8,0–8,3 g bei Soll 8,4 g** in `SIM FERTIG` bleiben.
2. `NACHM.` muss rechts erscheinen. Rechten TFT-Taster gedrückt halten.
   Erwartet: `[GBW B4b.2] Right hold topup start=1 (SSR OFF)`,
   `state=2`, `logicMotorRequest=1` (ausschließlich logisch!).
3. Rechten Taster loslassen, bevor 3 Sekunden vergangen sind. Erwartet:
   `[GBW B4b.2] Right raw release stop=1 (SSR OFF)`, dann `state=3`,
   `reason=3`, `logicMotorRequest=0`, danach `state=4`.
4. Während einer weiteren kurz gehaltenen Nachmahlsimulation ein
   kleines Prüfgewicht bis mindestens 8,35 g hinzufügen. Die Logik
   muss automatisch stoppen, `motorRequest=0`; längeres Halten darf
   nicht wieder starten.
5. Bei erreichter/überschrittener Zielmenge, nach Encoder-Kurzklick
   (UserCancelled) oder nach Entfernen des Siebträgers darf `NACHM.`
   nicht verfügbar sein.
6. Nach Neuauflegen soll ein regulärer Langdruck weiterhin funktionieren.

Der 3000-ms-Timeout kann bei Bedarf rein auf Host-Ebene geprüft werden.
Kein absichtliches Trennen von HX711-Leitungen unter Spannung.

Host-Test ohne Arduino/SSR-Abhängigkeiten:

```
g++ -std=c++11 -Wall -Wextra -Werror -Isrc \
  src/grinder_controller.cpp src/grinder_profile_detector.cpp \
  src/grinder_cycle_gate.cpp test_host/grinder_topup_b4b2_test.cpp \
  -o grinder_topup_b4b2_test
./grinder_topup_b4b2_test
```

Der vollständige PlatformIO-Build und die reale HMI-Hardwareprüfung stehen
noch aus; **vorher nicht committen** und keine Mühle anschließen.
