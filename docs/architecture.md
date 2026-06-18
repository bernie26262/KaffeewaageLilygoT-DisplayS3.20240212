# Architekturueberblick

## Ziel

Die Kaffeewaage soll als eigenstaendiges ESP32-Geraet funktionieren und gleichzeitig eine moderne WebUI bereitstellen. Die lokale TFT-/Encoder-Bedienung bleibt erhalten, die WebUI dient als komfortable Bedien- und Wartungsoberflaeche.

## Hauptmodule

### `main.cpp`

Enthaelt weiterhin die zentrale Hardware- und Ablaufsteuerung:

- HX711-/LoadCell-Initialisierung und Messwertverarbeitung
- TFT-Ausgabe
- Rotary-Encoder- und Tasterlogik
- Autodetect-State-Machine
- Tara-/Save-Ablauf
- Umschaltung zwischen Single-Dose- und Shot-Modus
- Verarbeitung von BLE-Kommandos und Shot-Ereignissen
- Wartungs-/Warnlogik
- WLAN-Start und Webserver-Initialisierung

Neue oder ausgelagerte Funktionen sollen moeglichst nicht mehr direkt in `main.cpp` wachsen, sondern in eigene Module wandern.

### `app_state.h`

Definiert den zentralen Zustand, der fuer die WebUI serialisiert wird. Ziel ist, dass `main.cpp` die Hardware- und Fachwerte aktualisiert und `coffee_web.cpp` daraus JSON fuer WebSocket-Clients erzeugt.

Zum State gehoeren inzwischen auch:

- aktiver Waagenmodus (`single_dose` oder `shot`)
- Shot-Zustand, Zeit, Gewicht, Flow und Messpunktanzahl
- BLE-Status, Notify-Rate, Zaehler und RAM-Diagnoselog

### `coffee_web.cpp/.h`

Enthaelt:

- eingebettete WebUI
- WebSocket-Route `/ws`
- WebSocket-Kommandos
- JSON-State-Erzeugung
- inkrementeller Shot-Messpunkt-Endpunkt `/api/shot/samples`
- PWA-Manifest-Handler
- Icon-/Favicon-Routen

Die Hauptseite ist aktuell bewusst noch nicht nach SPIFFS ausgelagert, damit keine groessere WebUI-Umstrukturierung entsteht.

### `ble_scale.cpp/.h`

Kapselt die WeighMyBru-kompatible BLE-Schnittstelle:

- GATT-Service und Characteristics
- Gewichtsmeldungen mit 5 Hz
- Empfang von TARE-, START-, STOP- und RESET-Kommandos
- Verbindungs-/Advertising-Status
- kurzes Diagnose-Ringlog im RAM

Das rohe serielle BLE-Logging ist standardmaessig deaktiviert und kann bei verfuegbarem USB gezielt mit `BLE_SCALE_VERBOSE_LOGS=1` aktiviert werden.

### `shot_session.cpp/.h`

Kapselt die fachliche Shot-Session unabhaengig von TFT und WebUI:

- nach Tara auf den ersten sicher erkannten Tropfen warten
- Shot automatisch starten und bei ausbleibendem Gewichtszuwachs beenden
- Zeit, Spitzen-/Endgewicht und geglaettete Flowrate berechnen
- bis zu 1200 Messpunkte im RAM halten
- letzten abgeschlossenen Shot bis zum Start des naechsten Shots erhalten

Der Shot-Verlauf wird nicht in NVS gespeichert und ist nach einem Neustart verloren.

### `coffee_ota.cpp/.h`

Enthaelt die Update-Seite `/update` mit getrenntem Upload fuer:

- Firmware (`U_FLASH`)
- SPIFFS-Dateisystem (`U_SPIFFS`)

Der ESP rebootet nach Upload nicht automatisch. Der Neustart erfolgt per Button.


### Zukuenftiges Modul: `coffee_wifi.cpp/.h`

Die WLAN-Konfiguration soll langfristig nicht mehr ueber fest einkompilierte `wifi_secrets.h` erfolgen, sondern ueber WLAN-Provisioning mit temporaerem Setup-Access-Point.

Geplantes Ziel:

- WLAN-Daten in NVS/Preferences speichern
- bei fehlenden oder ungueltigen WLAN-Daten einen Setup-Access-Point starten
- einfache Einrichtungsseite zum Speichern von SSID und Passwort bereitstellen
- `wifi_secrets.h` zunaechst als Entwicklungs-/Fallback-Option behalten und spaeter aus dem normalen Build entfernen

Details stehen in `docs/wifi-provisioning.md`.

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

- Wechsel zwischen Single-Dose-, Shot-, Daten- und Einstellungsansicht
- Tara und Save Dose
- Zielgewicht setzen
- Autodetect ein/aus
- Kalibriergewicht setzen
- Kalibrierfaktor speichern
- Gefaess messen/speichern/loeschen
- Wartungszeitpunkte zuruecksetzen
- Gesamtwerte bearbeiten

## Datenfluss der Shot-Waage

```text
HX711
  |---> ble_scale (5 Hz) ------------------------> Kaffeemaschine
  +---> Shot-Gewichtsfilter -> shot_session
                                  |---> AppState -> WebSocket-Status
                                  +---> RAM-Punkte -> /api/shot/samples
```

`main.cpp` koordiniert die Module. `shot_session` kennt weder TFT noch Webserver. `coffee_web.cpp` zeichnet den Graphen aus den per HTTP nachgeladenen RAM-Messpunkten; der kompakte Live-Status kommt weiterhin ueber WebSocket.

Details stehen in `docs/ble-shot-scale.md`.

## Aktivitaetsquellen und Backlight

WebUI-Kommandos duerfen Zustandswerte und die vorbereitete HMI-Seite aendern, gelten aber nicht als lokale Bedienung. Sie wecken das ausgeschaltete Display nicht und verlaengern den Display-Timeout nicht.

Lokale Encoder-/Tasterbedienung und eine signifikante Gewichtsveraenderung von mindestens 30 g bleiben Wakeup-Quellen.

## Grundsaetze fuer weitere Refaktorierung

- Kleine Diffs bevorzugen
- Build nach jedem Schritt
- WebUI nur vorsichtig anfassen
- keine grossen JS-Umsortierungen ohne konkreten Anlass
- keine direkte neue Preferences-Logik in `main.cpp`
- neue Webserver-/OTA-/Storage-Themen in eigene Module auslagern
