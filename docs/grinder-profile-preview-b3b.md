# Phase B3b – reale HX711-Profildiagnose (ohne Tara / SSR)

## Warum bewusst passiv?

Die Waage ist noch nicht zuverlässig kalibriert; `weightST[]` und
`weightTrichter[]` können aus der alten Legacy-Firmware beziehungsweise aus NVS
stammen und müssen an der realen Grind-by-Weight-Hardware geprüft werden.
Automatisches Tarieren aufgrund dieser möglicherweise unpassenden Werte wäre
verfrüht. Diese Phase macht die neue Erkennung testbar, ohne Betriebszustände
zu verändern.

## Verhalten

- Beim Start werden die bestehenden `weightST[4]`, `weightTrichter[4]` und
  `setWeightST[4]` einmalig aus dem bereits vorhandenen NVS-Datenmodell in den
  B2-Detektor geladen. **Keine zusätzlichen NVS-Schlüssel.**
- `GrinderWeightStability` aus B3a liefert echte `stable`-Werte zu jedem
  frischen `LoadCell.update()`-Sample.
- Der B2-Detektor läuft mit `previewOnly=true`. Vollständige Kombinationen
  führen zu `CombinationPreview`, **nicht** zu `AwaitTare` oder `ReadyAuto`.
- In der seriellen Diagnose erscheint z. B.:

  `[GBW B3b] WaitFunnel (Trichter auflegen) profile=1 target=8.4 g (preview; NO TARE)`

  `[GBW B3b] CombinationPreview (Tara nur simuliert) profile=1 target=8.4 g (preview; NO TARE)`

- Wenn Autodetect in den vorhandenen Einstellungen AUS ist, bleibt der Detektor
  im manuellen Auswahlmodus; dessen *Profilwahl* wird übernommen, aber
  `requestManualTare()` nicht aufgerufen.
- Die **alte** Legacy-`Autodetect()`-Schleife wird in dieser Branch-Phase
  nicht mehr ausgeführt. Sie hätte sonst unabhängig vom neuen Detektor
  `doTara()` aufgerufen und die Beobachtung verfälscht. Explizite manuelle
  Tara-Bedienung, Kalibrier- und Web-Wizard-Funktionen bleiben vorhanden.
- Bei aktiver Kalibrier-/Web-Wizard-Phase pausiert die Erkennung.
- B3b ruft **weder** `doTara()` noch GPIO16 oder `grinderRelay...` auf und
  verändert keine Sollgewichte oder HMI-Menüs. Profildaten bei Änderungen
  in HMI/WebUI werden derzeit erst nach Neustart neu eingelesen.

## Hardwaretest

1. Ohne Mühle und ohne Netzspannung am Steuergehäuse über USB starten.
2. Den Kalibrierfaktor mit bekannter Masse prüfen bzw. kalibrieren.
3. Autodetect EIN setzen und ein bekanntes Siebträger-Eigengewicht auflegen.
4. Im seriellen Monitor nach ca. 1,5 s `WaitFunnel` beobachten.
5. Trichter ergänzen: Nach weiteren ca. 1,5 s `CombinationPreview`.
6. Gefäß entfernen: Zustand sollte zu `WaitVessel` zurückkehren.
7. Unbekanntes Gefäß: `UnknownVessel`, keine Tara.

*Wichtig:* Falls die Profilgewichte noch nicht korrekt eingemessen sind,
werden die Testfälle 3–5 erwartungsgemäß als `UnknownVessel` angezeigt.
Diese Phase implementiert noch keine Einmessoberfläche für Siebträger und
Trichter und keine neue TFT-Statusanzeige. Zunächst müssen die tatsächlichen
Gewichte und die bestehende Datenhaltung verifiziert werden.

## Spätere Schritte

Nach verifizierter Kalibrierung und Profilgewichten: gezielte tatsächliche
HX711-Tarierung, Quittierung erst nach neuer stabiler Nullmessung, Profilauswahl
und TFT-Status; anschließend unabhängige Abschaltprüfungen. Realen SSR-Ausgang
vor Abschluss der elektrischen AUS-/Reset-Tests niemals freigeben.
