# B4b.1 – Profilbezogene Nachlaufkorrektur (RAM, nur Simulation)

**Ausgangsstand:** `72946f4` auf Branch `feature/grind-by-weight`, ZIP vom
22.10.2026, 16:37 Uhr. B4a einschliesslich Hardwaretests ist abgeschlossen.

## Ziel und Umfang

Der B1-`GrinderController` berechnet bereits beim regulaeren Sollgewicht-Stopp
nach 450 ms einen Nachlaufwert (`latencyG`). Der bisherige Aufruf von
`controller_.reset()` nach bestaetigter Gefaessentnahme verwirft ihn jedoch.

B4b.1 speichert **ausschliesslich im RAM der `GrinderCycleGate`** einen
Nachlaufwert je der vier Siebtraegerprofile. Der Anfangswert jedes Profils
entspricht B1: **0,25 g**. Das beim Start ausgewaehlte Profil erhaelt genau
seinen eigenen Wert, ohne den anderer Profile zu veraendern.

Der RAM-Wert wird nur nach `Complete` mit
`StopReason::TargetReached` und **bestaetigter Entnahme/Nullstellung**
aus dem Controller uebernommen. Manuelle Abbrueche und Fehler trainieren
**nicht**. Eine neue Auflage allein darf wie bisher keinen Neustart
ermoeglichen. Die bisherige Fehlerquittierung bleibt erforderlich.

Die bestehende B1-Lernformel und die Grenze von 0 bis 2 g sind unveraendert.
Ausserhalb dieses Bereichs faellt B1 wie bisher auf 0,25 g zurueck.

## Testbarkeit und Diagnose

In `main.cpp` erscheinen zwei zusaetzliche Meldungen:

- `[GBW B4b.1] settled profile=1 latency=0.55 g (RAM only, SSR OFF)`
  nach einem regulaer abgeschlossenen Mahlvorgang;
- `[GBW B4b.1] start profile=1 latency=0.55 g (RAM only, SSR OFF)`
  beim naechsten Start desselben Profils nach vollstaendiger Entnahme.

Die Zahlen sind **nur Beispiele** und haengen von den aufgelegten
Pruefgewichten ab. Im Zustand `Complete` ist der Wert bereits in B1
berechnet, wird aber erst bei bestaetigter Entnahme in den profilbezogenen
RAM-Speicher uebernommen.

`test_host/grinder_latency_b4b1_test.cpp` prueft unter anderem:

- Anfangswert 0,25 g und Lernen nach normalem Sollgewicht-Stopp;
- Behalten des Werts ueber den Entnahme-/Tara-Zyklus;
- tatsaechlich veraenderte Abschaltschwelle im Folgezyklus;
- getrennte Nachlaufwerte verschiedener Siebtraegerprofile;
- kein Lernschritt nach manuellem Abbruch oder Fehler;
- keine automatische Wiederfreigabe und keine persistente Speicherung
  beim Neustart (neue Instanz).

Beispiel fuer den eigenstaendigen PC-Test:

```sh
g++ -std=c++11 -Wall -Wextra -Werror -Isrc \
  src/grinder_controller.cpp src/grinder_profile_detector.cpp \
  src/grinder_cycle_gate.cpp test_host/grinder_latency_b4b1_test.cpp \
  -o grinder_latency_b4b1_test
./grinder_latency_b4b1_test
```

## Hardware-Regressionstest (USB-only, Muehle elektrisch getrennt)

1. Vollstaendigen PlatformIO-Build und Upload einschliesslich
   Hash-Verifikation durchfuehren.
2. Profil 1 mit Trichter vorbereiten, per Encoder lang starten, mit
   Pruefgewichten langsam zur Abschaltschwelle gehen, Abschluss abwarten.
   Im Serial-Log die Zeile `settled ... latency=...` notieren.
3. Siebtraeger vollstaendig entnehmen, die automatische Tara bis
   `WaitVessel` abwarten, **dasselbe Profil** erneut vorbereiten.
   Bei erneutem Simulationsstart muss die protokollierte `start`-Latenz
   dem zuvor errechneten `settled`-Wert entsprechen.
4. Ein anderes Profil vorbereiten (sofern vorhanden): Der dortige
   `start`-Wert muss von dessen bisherigem Lernstand abhaengen,
   beim ersten Start 0,25 g.
5. HMI-Abbruch per kurzem Encoder-Klick und Sicherheitsabbruch beim
   Abheben als Regression pruefen. Die logische Motoranforderung muss
   nach dem Stopp weiterhin auf 0 gehen; keine SSR-Ansteuerung.

## Wichtige Grenzen

- Die mit **manuell aufgelegten Pruefgewichten** gelernten Werte sind
  *keine* realistischen Kennwerte der Demoka-Muehle. Insbesondere kann
  das Ergebnis von der Art und Geschwindigkeit des Auflegens abhaengen.
- Nach einem Reset oder Neustart gelten fuer alle Profile wieder 0,25 g.
  Es werden keine NVS-Daten geschrieben.
- Die spaetere Auswertung echter Muehlen-Nachlaufwerte und ihre
  gegebenenfalls dauerhafte Speicherung erfordern einen eigenen,
  sorgfaeltig getesteten Entwicklungsschritt.
- HMI-Bedienung, WebUI-Kommandos, Tara, Skip, SSR/GPIO16 und
  Sicherheitsverriegelungen bleiben unveraendert. `latencyG()` ist
  nur ein Diagnosewert und **keine** physische Motorfreigabe.
- Vor B4c muss eine separate elektrische Sicherheitspruefung erfolgen.
