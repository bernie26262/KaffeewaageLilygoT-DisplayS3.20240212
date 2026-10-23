# B4c.1 – Vorbereitete Ausgangssperre (SSR bleibt AUS)

**Basis:** Commit c28a447 auf feature/grind-by-weight (ZIP vom
23.10.2026, 15:22 Uhr). Erst nach Host-, Build- und USB-Tests committen.
Die WebUI-Funktionsangleichung bleibt eine eigene Aufgabe.

## Ziel und Geltungsbereich

Ein zweiter, von GrinderCycleGate getrennter Entscheidungsweg prüft, ob
eine logische Motoranforderung grundsätzlich an eine künftige Ausgangsschicht
weitergegeben werden **dürfte**. In B4c.1 wird das Ergebnis *ausschließlich
protokolliert*. Die vorhandene grinder_relay.cpp bleibt unverändert und
enthält weiterhin nur grinderRelayInitOff(). Es gibt **keinen Codepfad,
der GPIO16 auf LOW setzt**.

Die serielle Diagnose unterscheidet:

- [GBW B4a.3b] logicMotorRequest=1 (SSR OFF): bisheriger Controller;
- [GBW B4c.1] wouldPermit=1 inhibit=0 (GPIO16 HIGH, SSR OFF):
  zusätzliche Sicherheitsentscheidung wäre freigabefähig, ist aber **nicht
  mit dem SSR verbunden**;
- [GBW B4c.1] wouldPermit=0 inhibit=N (GPIO16 HIGH, SSR OFF):
  Ausgangssperre aktiv beziehungsweise kein Motorwunsch.

Die inhibit-Codes (enum): 0 keine Sperre, 1 keine Anforderung,
2 ungültiger Kontext, 3 Sensor nicht frisch, 4 WebUI-Abbruch,
5 OTA/Neustart, 6 Lücke im Hauptloop, 7 Laufzeitgrenze,
8 unerwarteter Zustandswechsel.

## Entscheidung und Verriegelung

Der separate, host-testbare GrinderOutputGuard fordert:

1. Eine logische Anforderung aus GrinderCycleGate.
2. Passenden HMI-/Profil-/Ziel-/Wizard-Kontext und Bereitschaft.
3. Einen frischen HX711-Datensatz (maximal 1000 ms alt).
4. Keine noch ausstehende WebUI-Abbruchanforderung.
5. Keinen angenommenen Firmware-Upload und keine Neustartanforderung.
6. Keine Loop-Lücke über 500 ms seit einer vorherigen Anforderung.
7. Keine Überschreitung von 30 s normalem Mahlen bzw. 3 s Nachmahlen.

Wenn einer dieser Fälle unter logischer Anforderung scheitert, hält der
Guard seine Sperre fest. loop() ruft dann den vorhandenen
GrinderCycleGate.requestSafetyAbort() auf. Eine erneute Freigabe wird
erst nach vollständig zurückgesetzter Vorbereitung und expliziter
neuer Bedienhandlung möglich. OTA- und Neustartsperren bleiben bis
zum Neustart gesetzt.

Die OTA-Sperre wird bereits zu Beginn eines **gültigen Firmware-Uploads**
beziehungsweise bei einer Neustartanforderung atomar gesetzt. Ungültige
Firmware-Dateinamen lösen keine neue Sperre aus.

### Wichtig: Noch keine unabhängige Motorabschaltung

Der Guard erkennt eine blockierte loop()-Ausführung **erst, wenn loop()
wieder läuft**. Er kann während des Stillstands keinen physischen
SSR-Ausgang abschalten. Das atomare OTA-Signal wird ebenfalls erst
im Hauptloop verarbeitet. Vor einer echten Motorfreigabe müssen diese
Grenzen mit einer unabhängig wirkenden Abschaltung abgesichert werden.
Reset, Stromverlust und die Eingangsbeschaltung des aktiven LOW-SSR
sind durch B4c.1 ebenfalls nicht abgedeckt.

## Host-Test

Ohne Arduino, Relais oder Netzspannung (im Projektverzeichnis):

    g++ -std=c++11 -Wall -Wextra -Werror -Isrc \
      src/grinder_output_guard.cpp \
      test_host/grinder_output_guard_b4c1_test.cpp \
      -o grinder_output_guard_b4c1_test
    ./grinder_output_guard_b4c1_test

Geprüft werden Start/Stop/Nachmahlen, Kontext, Sensor, WebUI-Abbruch,
OTA-Sperre, 500-ms-Loop-Lücke, 30-s-/3-s-Grenzen, Moduswechsel,
Rücksetzung nach vollständiger Entnahme und millis()-Überlauf.

## USB-Hardwaretest – **Mühle weiterhin vollständig getrennt**

1. Über USB starten, HMI und WebUI normal aufrufen. GPIO16 wird
   ausschließlich auf HIGH initialisiert.
2. Kombination auflegen, Auto-Tara bis Bereit, Encoder lang drücken.
   Log: logicMotorRequest=1 und zusätzlich wouldPermit=1,
   physischer Ausgang weiterhin AUS.
3. Prüfgewichte bis zur Zielschwelle hinzufügen. Danach muss
   wouldPermit=0 und der Zustand SIM FERTIG erscheinen.
4. Bei hinreichendem Defizit NACHM. verwenden und loslassen.
   Während SIM NACHMAHLT wäre wouldPermit=1, nach Loslassen 0.
5. Abheben: kompletter Reset, neuer Zyklus darf normal beginnen.
6. Optional: Einen Test-OTA-Upload mit bereits geprüfter Firmware
   durchführen. Die Sperre inhibit=5 wird gesetzt und der ESP erst
   nach manueller Aufforderung neu gestartet.

## Noch offen vor echter Freigabe

- Elektrische SSR-Eingangsschaltung einschließlich Spannung an GPIO16,
  Boot-/Reset- und ESP-Ausfallzustand prüfen.
- Unabhängige Abschaltmöglichkeit bei blockiertem Hauptloop definieren.
- OTA-/Neustart- und Not-Aus-Abschaltung zeitlich und elektrisch messen.
- Erst danach separater Freigabe-Patch; niemals automatisch durch die
  B4c.1-Diagnose.
