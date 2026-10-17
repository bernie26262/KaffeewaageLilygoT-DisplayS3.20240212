# Phase B3d – automatische Tara und Profilauswahl (SSR gesperrt)

Ausgangspunkt: Branch `feature/grind-by-weight`, Commit `df7c7ea`.

## Ablauf

- B3a liefert ausschließlich frische HX711-Samples und die echte Stabilität.
- Bei `WaitFunnel` wird das erkannte Siebträgerprofil zur Laufzeit ausgewählt.
  `selectedST` bestimmt die Sollgewichtanzeige auf TFT und WebUI. Die automatische
  Auswahl wird NICHT als Bedienerauswahl in NVS gespeichert.
- Eine vollständige, stabile Kombination (auch direkt aufgelegt) erzeugt exakt
  **eine** `AutoCombination`-Tara-Anforderung aus dem B2-Detektor.
- Der Adapter ruft ausschließlich `doTara()` auf (bestehende HX711-Tara), meldet
  deren wirklichen Status und setzt die Gewichtsstabilität zurück.
- Erst nach einer NEUEN stabilen Nullmessung folgt `ReadyAuto` (`ready=1`).
- Wird die Kombination entfernt, erlischt `ReadyAuto` sofort. Nach bestätigtem
  Abheben wird ein eigener `AfterRemoval`-Tara-Auftrag verarbeitet; danach ist
  der Detektor wieder in `WaitVessel`.
- Bei Autodetect AUS kann der Benutzer weiterhin einen Siebträger manuell
  auswählen und über die bisherige WebUI-/TFT-Tara tarieren. Nur eine vorher
  stabile Messung mit gültigem Profil wird später als `ReadyManual` bestätigt.
- Fremde Tara-Aktionen (Kalibrierassistent etc.) verwerfen eine vorherige
  Mahlbereitschaft. Während Wizard, Shot-Modus und Initialisierung ist die
  automatische Erkennung pausiert.
- WebUI `weight.stable` verwendet jetzt das echte B3a-Stabilitätssignal.

## Sicherheitsabgrenzung

Die Ausgabe `ready=1` bedeutet **nur eine abgeschlossene Mahlvorbereitung**.
Der SSR-Ausgang GPIO16 wird in dieser Phase **niemals eingeschaltet**. Weder
`grinder_controller` noch eine SSR-ON-Funktion sind an diesen Ablauf gebunden.
Ein Softwaretest beweist keine sichere Relaisabschaltung während Reset/Boot.

## Hardwaretest (Mühle abgesteckt, Steuergehäuse ohne Netzspannung)

1. Waage ohne Gefäß starten; `WaitVessel` und nahe null g.
2. Eingemessenen Siebträger auflegen: `WaitFunnel`, korrektes Profil und
   Sollgewicht auf TFT/WebUI, Hinweis „Trichter auflegen“.
3. Trichter ergänzen: `CheckingFunnel` → `AwaitTare` → `WaitPostTare`
   → bei stabilem Nullpunkt `ReadyAuto ready=1`. Genau eine Tara.
4. Kombination abheben: `CheckingRemoval` → `AfterRemoval`-Tara →
   `WaitVessel ready=0`; bei erneutem Auflegen wieder Erkennung.
5. Zusammengesetzte Kombination direkt auflegen: ebenso exakt eine Tara.
6. Autodetect AUS: unbekanntes geeignetes Gefäß, Profil manuell wählen,
   Tara drücken; stabile Nullmessung führt zu `ReadyManual`.
7. WebUI-Messassistent weiterhin testbar; er darf keine konkurrierende Tara
   oder automatische Profilwahl auslösen.
8. Keine Neustarts, niemals ein SSR-Einschaltkommando.

Falls `Fault` oder unerwartete Tara auftritt: Test abbrechen, Monitorlog sichern
und keine Mühlenfreigabe vornehmen.
