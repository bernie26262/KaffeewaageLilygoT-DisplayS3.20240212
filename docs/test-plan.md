# Testplan

## Build-Test

Nach jedem Diff:

```powershell
pio run -e lilygo-t4-s3-lvgl
```

Erwartung:

- Build ist gruen
- keine neuen Fehler
- neue Warnungen nur bewusst akzeptieren

Hinweis: Für den aktuellen T4-S3-Branch nicht nur `pio run` verwenden, weil sonst je nach `platformio.ini` auch das Legacy-Environment gebaut werden kann.

Für einen stabilen Referenzstand zusätzlich im Dependency Graph prüfen:

```text
HX711_ADC          1.2.12
ESPAsyncWebServer  3.11.0
AsyncTCP           3.4.10
ArduinoJson        7.4.3
```

Abweichungen von diesen Versionen vor einem Release/Tag bewusst bewerten.

## WebUI-Basistest

Im Browser oeffnen:

```text
http://<ip>/
```

Pruefen:

- Seite laedt
- WebSocket verbindet sich
- Gewicht wird aktualisiert
- Statusanzeige plausibel
- Zielgewicht kann eingegeben und gespeichert werden
- Buttons reagieren
- Log zeigt keine unerwarteten Fehler

## PWA-/Icon-Test

Direkte URLs pruefen:

```text
http://<ip>/manifest.json?v=99
http://<ip>/icon-192.png?v=99
http://<ip>/icon-512.png?v=99
http://<ip>/favicon.ico?v=99
```

Erwartung:

- Manifest liefert gueltiges JSON
- beide PNG-Icons werden angezeigt
- Favicon wird geladen
- Browser-Tab zeigt das Icon

Android-Test:

1. Seite in Chrome oeffnen
2. Menue oeffnen
3. Zum Startbildschirm hinzufuegen
4. Homescreen-Icon pruefen
5. App starten
6. Anzeige im Standalone-Modus pruefen

Bei Icon-Problemen:

- Browser-Cache umgehen, z.B. `?v=99`
- alte Homescreen-Verknuepfung loeschen
- neu hinzufuegen

## WLAN-Profil-, Failover- und Recovery-Test

Prüfpunkte nach Änderungen an `coffee_wifi.cpp`, `t4s3_wifi.cpp` oder der WLAN-WebUI:

1. WLAN1/2 speichern und anzeigen; Passwörter nicht im Klartext ausliefern; leeres Passwort beim Bearbeiten behält den vorhandenen Schlüssel.
2. Preferred auf WLAN2 setzen: laufende WLAN1-Verbindung bleibt bestehen. Nach Neustart verbindet die Waage WLAN2.
3. WLAN1 bevorzugt, aber vorübergehend unerreichbar: nach ca. 15 s erfolgt Failover zu WLAN2.
4. WLAN2 nach stabilem Betrieb abschalten, WLAN1 ebenfalls nicht erreichbar: nach Verbindungsversuchen erscheint der automatische **AP-only** `Waagen-Setup`.
5. Smartphone verbindet sich mit `Waagen-Setup`; die separate Seite `http://192.168.4.1/` bleibt mindestens 3 Minuten erreichbar.
6. WLAN2 wieder aktivieren und „Setup-WLAN beenden / WLAN erneut versuchen“ auslösen: Verbindung ohne ESP-Neustart.
7. Manuellen Setup-AP über HMI starten: `WIFI_AP_STA`, normale Heimnetz-WebUI bleibt erreichbar.
8. Im AP-only-Recovery keine letzte Ziel-SSID als *verbunden* auf dem HMI anzeigen; IP und RSSI ohne STA-Verbindung leer.
9. Nach Test ursprüngliche WLAN1-SSID und Preferred-Einstellung kontrollieren; Normalbetrieb wiederherstellen.

**Sicheres Testverfahren:** Statt das Heim-WLAN abzuschalten, WLAN1-SSID vorübergehend auf einen nicht existierenden Testnamen ändern (Passwort leer lassen) und WLAN2 über einen abschaltbaren Hotspot testen. Produktives FritzBox-/UniFi-WLAN dabei nicht ausschalten. Mit der einzigen funktionierenden Waage Änderungen kontrolliert durchführen.

**Bereits auf T4-S3 hardwaregetestet:** Preferred, Boot auf WLAN2, Failover, AP-only-Recovery, Setup-Seite, Retry über HMI und WebUI.

## Firmware-OTA-Test (kein Filesystem-OTA)

Update-Seite unter `http://<ip>/update` öffnen. Build-Datei: `.pio\build\lilygo-t4-s3-lvgl\firmware.bin`.

1. Ohne Datei: Upload-Button deaktiviert.
2. `bootloader.bin` auswählen: Fehlermeldung, Upload bleibt blockiert. Gültige `firmware.bin` anschließend auswählen: Fehler verschwindet, Button wird grün.
3. Während gültigem OTA-Upload: Fortschritt, Warnung und Navigation-Sperre prüfen. BLE-Verbindung soll beim Start dieses Uploads beendet werden.
4. Bei 100 %: Erfolgsmeldung und Neustartbereich; Datei und Upload gesperrt, „Zurück zur WebUI“ ausgeblendet.
5. Vor Neustart mit `Strg+F5` neu laden: Zustand bleibt gesperrt; `GET /update/status` meldet `{"reboot_required":true}`.
6. Auf „ESP32 jetzt neu starten“ klicken: Neustart-Hinweis erscheint und „Zurück zur WebUI“ wird wieder angeboten.
7. Nach Boot: Wiegen, Tarieren, Shot-Funktion, WLAN und WebUI prüfen; BLE muss wieder verfügbar sein; die OTA-Seite erlaubt erneut Dateiauswahl.
8. Browser-Assets `/manifest.json`, `/icon-192.png`, `/icon-512.png` und `/favicon.ico` separat prüfen – sie werden aus dem Firmware-Flash geliefert.

Serverseitig muss ein falscher Firmware-Dateiname HTTP 400 auslösen; ein zweiter Upload nach Erfolg und vor Neustart muss HTTP 409 ergeben. Diese Fälle bei Bedarf mit gezielten HTTP-Tests überprüfen, ohne produktive Firmwaredaten unnötig zu verändern.

**Nicht mit der einzigen Waage mutwillig erzwingen:** Stromunterbrechung oder ein abgebrochener OTA-Upload. Die Implementierung verwirft gegebenenfalls einen vorherigen `Update.isRunning()`-Zustand und beginnt einen neuen Upload vollständig bei 0 % (kein Byte-Resume). Dieser Sonderfall wurde noch nicht auf Hardware getestet.

`buildfs`, `uploadfs` sowie `spiffs.bin`/`littlefs.bin` und ein Filesystem-Upload sind in der T4-S3 nicht mehr vorgesehen.

## WebUI-Langlauf-Performance

Nach Änderungen an `src/coffee_web.cpp` oder den eingebetteten JS-Assets:

1. Browser-Konsole öffnen
2. WebUI mehrere Stunden offen lassen
3. Gewichtsanzeige und Wartungs-/Datenbereiche normal beobachten

Erwartung:

- keine dauernden Chrome-Meldungen `message handler took ... ms`
- WebSocket bleibt verbunden
- Gewichtsanzeige bleibt reaktiv
- Wartungs- und WLAN-Anzeigen aktualisieren weiter

Technischer Zielzustand: State-Nachrichten werden über `requestAnimationFrame` gebündelt und DOM-Schreibzugriffe werden nur bei tatsächlichen Änderungen ausgeführt.

## Storage-/Preferences-Test

Nach Storage-Aenderungen pruefen:

- gespeicherter Siebtraeger bleibt erhalten
- gespeichertes Zielgewicht bleibt erhalten
- Autodetect-Einstellung bleibt erhalten
- Gefaessgewichte bleiben erhalten
- Kalibrierwert bleibt erhalten
- Wartungs-/Statistikwerte bleiben erhalten

Kontrollsuche:

```powershell
Select-String -Path src\main.cpp -Pattern "preferences\.begin|preferences\.put|preferences\.get|preferences\.end" -CaseSensitive:$false
```

Erwartung: keine Treffer.

## Autodetect-/Save-Basistest

Autodetect EIN:

- HMI bewusst auf eine andere Seite stellen, z.B. Daten, Wartung oder WLAN
- WebUI auf Waage öffnen
- bekanntes Gefäß auflegen
- Anzeige muss unmittelbar auf den Lastsprung reagieren
- Auto-Tara soll nach stabiler Gefäßerkennung mit aktuell `800 ms` Bestätigung erfolgen; das globale Stable-Flag ist für die Erkennung nicht mehr Voraussetzung
- nach Tara Nullpunkt mindestens 30 s beobachten
- Save-Freigabe prüfen
- Gefäß abheben
- Leer-Tara darf erst bei negativer Änderung >= 30 g und stabilem Messwert erfolgen
- Save wird gesperrt

Autodetect AUS:

- HMI bewusst auf eine andere Seite stellen
- über WebUI manuell Tara auslösen
- Save-Freigabe prüfen
- Dose speichern
- Statistikwerte prüfen
- Gewicht entfernen
- Save wird wieder gesperrt

Während Gefäß-Wizard, Kalibrier-Wizard oder Web-Wizard aktiv sind, muss Autodetect weiterhin pausiert bleiben.

## Wartung aktiv/inaktiv

Test je Wartungstyp Kaffeemaschine, Mühle und Filter:

1. Wartung aktiv lassen und Save Dose auslösen
2. Gesamtwerte und jeweilige Wartungszähler steigen
3. Wartung deaktivieren
4. Save Dose erneut auslösen
5. Gesamtwerte steigen weiter
6. Zähler der deaktivierten Wartung bleiben unverändert
7. WebUI zeigt bei Shots/Mahlgut `disabled`
8. HMI zeigt bei Shots/Mahlgut `-`
9. Wartung reaktivieren
10. Zähler laufen ab altem Stand weiter

## HMI-Gewichtsanzeige

- Kleine negative Werte zwischen `-0,05 g` und `0,0 g` dürfen nicht als `-0,0 g` erscheinen, sondern als `0,0 g`.
- Bei echten negativen Werten ab gerundet `-0,1 g` muss das Minuszeichen sichtbar bleiben.
- Leere Waage soll im Stable-Zustand ruhig bei `0,0 g` bleiben.
- Einzelne Bohnen mit etwas über `0,1 g` sollen eine sichtbare Änderung auslösen.
- Größere Gewichtsänderungen beim Auflegen/Abheben sollen schnell nachlaufen.

## TFT-/HMI-Beobachtung

Ein einmaliger Grafikfehler wurde beobachtet:

- Farben invertiert
- heller/weißer Hintergrund statt dunklem Design
- bisher nicht reproduzierbar

Aktueller Umgang:

- nur beobachten
- nicht ohne Reproduktion umbauen
- bei erneutem Auftreten Foto, Zeitpunkt und Bedienkontext notieren


## BLE-/Gaggiuino-Basistest

Monitor:

```powershell
pio device monitor -e lilygo-t4-s3-lvgl -b 115200 | Select-String "\[BLE\]|\[T4S3\]\[BLE\]|\[T4S3\]\[SHOT\]|Guru|abort|Brownout|Reboot"
```

Prüfen:

1. BLE startet etwa fünf Sekunden nach Boot.
2. Ohne Maschine: Status blau/`bereit`, Advertising aktiv.
3. Mit Maschine: Status grün/verbunden.
4. Disconnect startet Advertising erneut.
5. Tara von Gaggiuino wird auf der Shot-Seite ausgeführt.
6. Tara von Gaggiuino wird im Single-Dose-Modus ignoriert.
7. WLAN, WebUI und HX711 bleiben während BLE-Betrieb stabil.

Bekannte Einschränkung: Beim getesteten Gaggiuino-Client werden keine `START`, `STOP` oder `RESET`-Kommandos gesendet. Ein Aus-/Einschalten der Bluetooth-Scale-Funktion auf der Maschine kann auf Gaggiuino-Seite zu einem Neustart führen. Die Waage selbst zeigt dabei bisher keinen Guru, Abort oder Brownout.

## Shot-Automatik und Flowrate

### Tara ohne Shot

1. Shot-Seite öffnen.
2. Tara über HMI, WebUI oder Maschine auslösen.
3. Status zeigt `wartet auf Bezug`.
4. 45 Sekunden nichts auflegen/einlaufen lassen.
5. Erwartung: Armed endet, Session geht in Bereitschaft zurück.
6. Ein zuvor abgeschlossener Graph bleibt erhalten.

### Realer Shot

1. Tara auslösen.
2. Bezug starten.
3. Pumpenvibration allein darf keinen `SHOT_START` erzeugen.
4. Timer startet beim ersten plausibilisierten Flüssigkeitszuwachs; bei langsamem ersten Tropfen kann dies etwas nach dem optisch sichtbaren Tropfen liegen.
5. Gewicht, Zeit und Flowrate laufen auf HMI/WebUI.
6. `SHOT_FALSE_START` darf bei einem normalen echten Shot nicht auftreten.
7. Nach Ende des relevanten Gewichtszuwachses stoppt die Session automatisch.
8. Endzeit, Endgewicht und Graph bleiben sichtbar.
9. Pre-Infusion vor dem plausibilisierten Flüssigkeitszuwachs ist bewusst nicht Teil dieser Zeit.
10. Abschließendes Gaggiuino-Tara nach dem Ausschalten des Bezugs muss den nächsten Shot wieder armeren, ohne den gespeicherten Graphen sofort zu löschen.

### Zweiter Shot

1. Nach abgeschlossenem Shot erneut Tara auslösen.
2. Vorheriger Graph bleibt während Armed sichtbar.
3. Erst beim echten Start des neuen Shots wird der alte Puffer überschrieben.

### Plausibilität Flowrate

- blaue Flowrate-Kurve soll den Trend zeigen und nicht jedem HX711-Zittern folgen
- Einschwingzeit von etwa 1 bis 2 Sekunden ist durch das 2,5-Sekunden-Regressionsfenster normal
- nach Stop wird der aktuelle Flow-Wert auf 0 gesetzt
- Gewichtsdaten an Gaggiuino bleiben von der Flowrate-Glättung unabhängig

## Weight-Diagnose / CSV

1. `Einstellungen -> System / OTA -> Gewichtsdiagnose` öffnen.
2. `Start` drücken und prüfen, dass Samplezahl/Dauer steigen.
3. Zwischen `System / OTA`, Home und Shot wechseln; die Aufnahme muss im ESP weiterlaufen.
4. Zurück zu `System / OTA`, `Stop` drücken und CSV herunterladen.
5. CSV muss Library-, Median-, FAST-, STABLE-, DISPLAY- und SHOT-Werte sowie Event-Spalte enthalten.
6. `Löschen` leert den Ringpuffer.
7. `Shot abbrechen` darf eine laufende/armed Shot-Session manuell auflösen, ohne Neustart des ESP.
8. Bei längeren Tests PSRAM-Kapazität prüfen: bevorzugt 9.000 Samples / 15 min, Fallback 1.200 Samples / 2 min.

## Pumpenvibrations-Test

Für Regression der Shot-Startlogik:

1. Tasse/Gefäß auf der Waage mehrere Minuten setzen lassen und tarieren.
2. Diagnose starten.
3. Vibrationspumpe ca. 10 s laufen lassen, ohne Wasser in das Gefäß auf der Waage einzuleiten.
4. Erwartung: deutliche Messwertunruhe ist zulässig, aber kein `SHOT_START`.
5. Anschließend echten Shot messen und prüfen, dass ein konsistenter positiver Flüssigkeitstrend weiterhin startet.

## Shot-Graph / RAM-Reconnect

1. Shot durchführen und Graph abwarten.
2. Während `armed/running` müssen Gewicht/Flow sichtbar flüssig aktualisieren; die kompakte Shot-Telemetrie läuft mit ca. 5 Hz (`200 ms`).
3. Der Full State wird währenddessen nur etwa einmal pro Sekunde gesendet; außerhalb eines Shots bleibt er bei 500 ms.
4. Browser neu laden oder WebSocket kurz trennen.
5. Erwartung: letzter Shot wird über `/api/shot/samples` wieder aus RAM geladen.
6. Tara ohne folgenden Shot darf den Graph nicht löschen.
7. ESP-Neustart darf den Graph bewusst löschen.

## Mehrgeräte- und HMI-Seitensynchronisierung

1. HMI auf Shot stellen.
2. WebUI auf einem neuen Gerät öffnen.
3. Erwartung: neue WebUI öffnet nach erstem State Shot.
4. Desktop auf Daten wechseln.
5. Erwartung: HMI und weiteres Mobilgerät wechseln ebenfalls auf Daten.
6. Einstellungen-Unterseiten ebenfalls prüfen.
7. OTA-Seite öffnen und über `Zurück zur WebUI` zurückkehren.
8. Erwartung: aktuelle HMI-Seite wird wiederhergestellt.

## Produktiv-Cache

1. Nach einer Firmware mit neuer `COFFEE_WEB_ASSET_VERSION` WebUI einmal neu öffnen.
2. Desktop und Mobilgerät prüfen.
3. Danach Browser schließen und erneut öffnen.
4. Erwartung: Assets laden schnell aus Cache, WebSocket verbindet normal.
5. Bei späteren WebUI-Änderungen zentrale Asset-Version erhöhen.
6. Browserdaten dürfen im Normalfall nicht manuell gelöscht werden müssen.
