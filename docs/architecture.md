# Architekturueberblick

## Ziel

Die Kaffeewaage soll als eigenstaendiges ESP32-Geraet funktionieren und gleichzeitig eine moderne WebUI bereitstellen. Die lokale TFT-/Encoder-Bedienung bleibt erhalten, die WebUI dient als komfortable Bedien- und Wartungsoberflaeche.

## Hauptmodule

### `main.cpp`

Enthaelt weiterhin die zentrale Hardware- und Ablaufsteuerung der Legacy-TFT-Version:

- HX711-/LoadCell-Initialisierung und Messwertverarbeitung
- TFT-Ausgabe
- Rotary-Encoder- und Tasterlogik
- Autodetect-State-Machine
- Tara-/Save-Ablauf
- Wartungs-/Warnlogik
- WLAN-Start und Webserver-Initialisierung

Neue oder ausgelagerte Funktionen sollen moeglichst nicht mehr direkt in `main.cpp` wachsen, sondern in eigene Module wandern.

### `app_state.h`

Definiert den zentralen Zustand, der fuer die WebUI serialisiert wird. Ziel ist, dass `main.cpp` die Hardware- und Fachwerte aktualisiert und `coffee_web.cpp` daraus JSON fuer WebSocket-Clients erzeugt.

### `coffee_web.cpp/.h`

Enthaelt:

- eingebettete WebUI
- WebSocket-Route `/ws`
- WebSocket-Kommandos
- JSON-State-Erzeugung
- PWA-Manifest-Handler
- Icon-/Favicon-Routen
- ausgelieferte WebUI-Teilassets `/coffee.css`, `/coffee_core.js`, `/coffee_render.js`, `/coffee_events.js`

Die Hauptseite ist aktuell bewusst noch nicht nach SPIFFS ausgelagert, damit keine groessere WebUI-Umstrukturierung entsteht. Die WebUI ist aber zur besseren Auslieferung und Performance in mehrere Firmware-Routen aufgeteilt. Der Browser rendert WebSocket-State-Nachrichten gebuendelt per `requestAnimationFrame` und schreibt DOM-Werte nur bei Aenderungen neu.

### `coffee_ota.cpp/.h`

Enthaelt die Update-Seite `/update` mit getrenntem Upload fuer:

- Firmware (`U_FLASH`)
- SPIFFS-Dateisystem (`U_SPIFFS`)

Der ESP rebootet nach Upload nicht automatisch. Der Neustart erfolgt per Button.


### `coffee_wifi.cpp/.h`

Buendelt die WLAN-Konfiguration. Der aktuelle Legacy-Stand unterstuetzt:

- WLAN-Daten in NVS/Preferences speichern
- gespeicherte WLAN-Daten explizit aktivieren/deaktivieren
- Rueckfall auf das Standard-WLAN aus `wifi_secrets.h`, wenn gespeicherte aktive Daten nicht verbinden
- gespeicherte WLAN-Daten loeschen
- manuellen Setup-Access-Point `Waagen-Setup` mit Setup-Seite unter `http://192.168.4.1/`
- WLAN-Signalqualitaet fuer WebUI und TFT-Header

`wifi_secrets.h` bleibt im Legacy-Projekt zunaechst als Entwicklungs-/Fallback-Option erhalten. Details stehen in `docs/wifi-provisioning.md`.

### `coffee_storage.cpp/.h`

Buendelt die persistenten NVS-/Preferences-Zugriffe.

`main.cpp` soll keine direkten `preferences.begin`, `preferences.put...`, `preferences.get...` oder `preferences.end`-Zugriffe mehr enthalten. Stattdessen werden Speicheroperationen ueber Funktionen wie `coffeeStorageSaveStats(...)` oder `coffeeStorageLoadOrInit(...)` ausgefuehrt.

## Webserver-Initialisierung

Wichtig ist die Handler-Reihenfolge:

1. Route `/`
2. WebUI-/PWA-Handler aus `coffeeWebBegin(server)`
3. OTA-Handler aus `coffeeOtaBegin(server)`
4. statische SPIFFS-Auslieferung `server.serveStatic("/", SPIFFS, "/")`
5. `server.begin()`

Der breite Static-Handler darf PWA-/Favicon-Spezialrouten nicht vorzeitig abfangen.

## WebUI-Kommunikation

Die WebUI verwendet WebSocket-Kommunikation. Der ESP sendet regelmaessig bzw. bei Zustandsaenderungen einen JSON-State. Kommandos der WebUI werden als kurze Befehle an den ESP gesendet und dort in `main.cpp` verarbeitet.

Beispiele fuer Kommandobereiche:

- Save Dose
- Zielgewicht setzen
- Autodetect ein/aus
- Kalibriergewicht setzen
- Kalibrierfaktor speichern
- Gefaess messen/speichern/loeschen
- Wartungszeitpunkte zuruecksetzen
- Wartungsintervalle setzen
- Wartungen aktivieren/deaktivieren
- Gesamtwerte bearbeiten
- WLAN-Daten speichern/aktivieren/deaktivieren/loeschen
- Setup-AP starten/stoppen

## Grundsaetze fuer weitere Refaktorierung

- Kleine Diffs bevorzugen
- Build nach jedem Schritt
- WebUI nur vorsichtig anfassen
- keine grossen JS-Umsortierungen ohne konkreten Anlass
- keine direkte neue Preferences-Logik in `main.cpp`
- neue Webserver-/OTA-/Storage-Themen in eigene Module auslagern


## Legacy-TFT und T4-S3/LVGL

Dieser Branch ist der Legacy-TFT-Stand. Die T4-S3-/LVGL-Variante wird im Branch `feature/t4s3-lvgl-touch` gepflegt. Gemeinsame Fachlogik wie Wartung, Autodetect, Storage und WebUI-Performance kann portiert werden, aber Diffs sollten wegen unterschiedlicher UI-Struktur nicht blind zwischen den Branches angewendet werden.

## Aktuelle Fachlogik-Hinweise

- Autodetect und Auto-Tara laufen unabhaengig davon, welche Legacy-TFT-Seite gerade sichtbar ist. WebUI-Bedienung darf dadurch nicht blockiert werden.
- Sonderablaeufe wie Gefaess-Wizard, Kalibrier-Wizard und Web-Wizard blockieren Autodetect weiterhin bewusst.
- Wenn eine Wartung deaktiviert ist, bleiben die zugehoerigen Shots-/Mahlgut-Zaehler gespeichert, werden bei `Save Dose` aber nicht weitergezaehlt. Nach Reaktivierung laufen sie ab dem alten Stand weiter.
- In der WebUI werden deaktivierte Wartungszaehler als `disabled` angezeigt; auf dem Legacy-TFT werden platzsparend Striche verwendet.
