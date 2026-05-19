# WLAN-Provisioning / Einrichtungsmodus

## Ziel

Die Kaffeewaage soll langfristig ohne fest einkompilierte WLAN-Zugangsdaten betrieben werden koennen.

Aktuell werden WLAN-Daten ueber `wifi_secrets.h` eingebunden. Fuer ein dauerhaft nutzbares oder spaeter verkaufbares Geraet ist das unpraktisch, weil jedes Geraet individuell kompiliert werden muesste und WLAN-Passwoerter nicht ins Repo gehoeren.

Stattdessen soll die Waage einen Einrichtungsmodus bekommen, in dem sie temporaer einen eigenen WLAN-Zugangspunkt bereitstellt. Der Nutzer verbindet sich mit diesem Zugangspunkt und traegt dort die SSID und das Passwort des Ziel-WLANs ein.

Der passende Begriff fuer diese Funktion ist:

```text
WLAN-Provisioning
```

oder genauer:

```text
SoftAP-Provisioning mit Konfigurationsportal
```

Oft wird dafuer auch der Begriff `Captive Portal` verwendet, wenn Mobilgeraete automatisch eine Einrichtungsseite anzeigen.

## Zielverhalten

Beim Start versucht die Waage, sich mit gespeicherten WLAN-Zugangsdaten zu verbinden.

### Fall 1: Zugangsdaten vorhanden und Verbindung erfolgreich

- Waage verbindet sich mit dem normalen WLAN.
- WebUI ist unter der normalen IP erreichbar.
- SoftAP ist aus.

### Fall 2: keine Zugangsdaten vorhanden

- Waage startet einen eigenen Access Point.
- Beispiel-SSID:

```text
Waagen-Setup
```

- Der Nutzer verbindet sich mit diesem WLAN.
- Eine Setup-Seite ist erreichbar, z.B.:

```text
http://192.168.4.1/
```

- Dort werden Ziel-SSID und WLAN-Passwort eingetragen.
- Die Daten werden in NVS/Preferences gespeichert.
- Die Waage startet neu oder verbindet sich anschliessend neu mit dem Ziel-WLAN.

### Fall 3: gespeicherte Zugangsdaten vorhanden, Verbindung scheitert

Empfohlenes Verhalten:

- Waage versucht fuer eine begrenzte Zeit, sich zu verbinden.
- Wenn kein Connect gelingt, startet sie den Setup-Access-Point.
- Das lokale HMI zeigt einen Hinweis wie:

```text
WLAN einrichten
WLAN: Waagen-Setup
Browser öffnen:
192.168.4.1
```

## Einrichtungsseite

Die Setup-Seite sollte sehr einfach sein:

- Titel: `WLAN einrichten`
- Eingabefeld SSID
- Eingabefeld Passwort
- Button `Speichern und verbinden`
- Hinweis, dass das Geraet nach dem Speichern neu verbindet oder neu startet

Optional spaeter:

- Netzwerkscan
- Anzeige gefundener SSIDs
- Button `WLAN-Daten loeschen`
- Button `Setup-Modus beenden`

## Speicherung

Die WLAN-Daten sollen intern in NVS/Preferences gespeichert werden. In der WebUI wird dafuer der nutzerfreundliche Begriff `gespeicherte WLAN-Daten` verwendet.

Aktuelle Keys im Modul `coffee_wifi`:

```text
Namespace: coffee_wifi
ssid
pass
active
```

Nach dem Phase-2a-Rueckschritt gilt: Eine nicht-leere SSID bedeutet nur, dass gespeicherte WLAN-Daten vorhanden sind. Automatisch verwendet werden sie nur, wenn zusaetzlich `active=true` gesetzt ist. Der sichere Entwicklungsstand setzt `active` beim Speichern bewusst nicht automatisch auf `true`.

Die WebUI soll keine technischen Begriffe wie `NVS` oder `wifi_secrets.h` anzeigen. Stattdessen gelten diese Anzeigen:

```text
NVS/Preferences        -> gespeicherte WLAN-Daten
wifi_secrets.h         -> Standard-WLAN aus Firmware
NVS-Daten vorhanden    -> Gespeicherte WLAN-Daten
NVS-Daten aktiv        -> Gespeicherte WLAN-Daten aktiv
```

Das echte WLAN-Passwort wird nicht an die WebUI gesendet. Die WebUI zeigt nur `Gespeichertes Passwort: vorhanden`.

Langfristig sollte diese Logik nicht in `main.cpp` wachsen, sondern in ein eigenes Modul ausgelagert werden, z.B.:

```text
src/coffee_wifi.h
src/coffee_wifi.cpp
```

Dieses Modul enthaelt in Phase 1/1b bereits:

- Laden der gespeicherten WLAN-Daten
- Fallback auf `wifi_secrets.h` / in der WebUI: `Standard-WLAN aus Firmware`
- Speichern neuer WLAN-Daten als vorbereitete, noch nicht automatisch aktive Daten
- Loeschen der WLAN-Daten
- Quelle der aktiven Zugangsdaten als Statusinformation
- Anzeige, ob gespeicherte WLAN-Daten vorhanden sind
- Start der normalen WLAN-Verbindung

Phase 2b ergaenzt eine sichere WebUI-Speicherfunktion:

- SSID und Passwort koennen intern in NVS abgelegt werden.
- Der Key `active` bleibt dabei bewusst `false`.
- Die aktive Verbindung bleibt daher weiterhin beim Standard-WLAN aus der Firmware, solange keine spaetere Validierungslogik `active=true` setzt.
- Nach dem Speichern sollte die WebUI anzeigen: `Gespeicherte WLAN-Daten: ja`, aber weiterhin `Quelle: Standard-WLAN aus Firmware`.

Phase 2c ergaenzt eine explizite Aktivierung:

- Gespeicherte WLAN-Daten koennen bewusst aktiviert oder deaktiviert werden.
- Aktivierte gespeicherte WLAN-Daten werden erst nach Neustart verwendet.
- Falls die Verbindung mit aktivierten gespeicherten WLAN-Daten scheitert, wird der Active-Marker geloescht und temporaer auf das Standard-WLAN aus der Firmware zurueckgefallen.
- Dadurch soll ein OTA-/WebUI-Lockout durch falsche gespeicherte WLAN-Daten verhindert werden.

Noch nicht enthalten:

- Start des Setup-Access-Points
- Verbindungstest vor dem Setzen von `active=true`

## Webserver und Routen

Da das Projekt bereits `ESPAsyncWebServer` nutzt, kann das Provisioning in die bestehende Webserver-Struktur integriert werden.

Moegliche Routen im Setup-Modus:

```text
GET  /
POST /wifi/save
GET  /wifi/status
POST /wifi/clear
```

Im normalen Betriebsmodus kann eine WLAN-Seite spaeter unter Einstellungen / System angeboten werden.


### Phase 3a: Manueller Setup-Access-Point

Der manuelle Setup-AP ist als sicherer Zwischenschritt vor einem echten Captive Portal gedacht. Er wird bewusst gestartet und laeuft parallel zur bestehenden WLAN-Verbindung im Modus `WIFI_AP_STA`. Dadurch bleiben WebUI und OTA im normalen WLAN erreichbar, waehrend das Setup-WLAN getestet werden kann.

```text
AP-SSID: Waagen-Setup
AP-IP:   192.168.4.1
Setup-Seite: http://192.168.4.1/
Alternativ:  http://192.168.4.1/wifi-setup
```

Die Setup-Seite speichert SSID und Passwort in Preferences/NVS und aktiviert diese gespeicherten WLAN-Daten fuer den naechsten Neustart. Das echte WLAN-Passwort wird weiterhin nicht im Klartext an die WebUI zurueckgegeben.

Dieser Schritt ist noch kein echtes Captive Portal: Es gibt noch keine DNS-Umleitung und kein automatisches Smartphone-Popup.

## Captive-Portal-Option

Ein vollstaendiges Captive Portal ist komfortabel, aber nicht zwingend fuer den ersten Schritt.

Minimaler erster Schritt:

- SoftAP starten
- feste Setup-IP anzeigen
- einfache Seite unter `http://192.168.4.1/`
- SSID/Passwort speichern

Spaeter kann DNS-Umleitung ergaenzt werden, damit Smartphones die Seite automatisch oeffnen.

## Sicherheit

Das Setup-WLAN sollte nicht dauerhaft aktiv bleiben.

Empfehlungen:

- Setup-AP nur starten, wenn keine WLAN-Daten vorhanden sind oder Verbindung fehlschlaegt.
- Optional Setup-AP nur fuer begrenzte Zeit aktiv lassen.
- Optional Setup-AP mit einfachem Geraete-Passwort schuetzen.
- WLAN-Passwort in Formularen als Passwortfeld anzeigen.
- Gespeicherte Passwoerter niemals in der WebUI im Klartext anzeigen.

## Bedeutung fuer das HMI

Auf dem lokalen HMI wird OTA nicht benoetigt, weil Firmware- und Dateisystem-Updates vom Computer aus erfolgen.

Stattdessen ist fuer ein eigenstaendiges Geraet ein WLAN-Einrichtungsmodus wichtig.

Fuer das HMI ist `pageID = 22` als Seite `WLAN / Netzwerk` reserviert. Nach aktuellem Layout stehen dort maximal fuenf Textzeilen zur Verfuegung. Die Seite soll daher nur Status und kurze Hilfe anzeigen, keine WLAN-Passworteingabe.

Beispiel Normalbetrieb:

```text
WLAN verbunden
Quelle: gespeicherte Daten
SSID: FRITZ!Box 6490 Cable
IP: 192.168.11.83
Setup ueber WebUI
```

Beispiel Setup-AP aktiv:

```text
WLAN-Setup aktiv
Mit Handy verbinden:
WLAN: Waagen-Setup
Browser oeffnen:
192.168.4.1
```

Damit kann der Nutzer das Geraet ohne erneutes Kompilieren in sein eigenes WLAN bringen; die eigentliche Eingabe erfolgt ueber Smartphone/Tablet in der WebUI.

## Umsetzbarkeit im aktuellen LilyGO T-Display S3 Projekt

Ja, das ist auch im bestehenden Projekt mit dem LilyGO T-Display S3 moeglich.

Das Projekt nutzt bereits:

- ESP32-S3 mit WLAN
- Preferences/NVS
- AsyncWebServer
- WebUI
- SPIFFS/OTA

Damit sind die technischen Voraussetzungen vorhanden.

Empfohlene schrittweise Umsetzung:

1. Neues Modul `coffee_wifi` anlegen. ✅
2. WLAN-Zugangsdaten aus NVS laden und bei fehlenden Daten auf `wifi_secrets.h` zurueckfallen. ✅
3. Speichern/Loeschen/Status der Zugangsdaten vorbereiten, ohne AP/WebUI bereits umzubauen. ✅
4. Nach Phase-2a-Test: gespeicherte WLAN-Daten nicht mehr ungeschuetzt automatisch aktivieren, sondern nur noch mit `active`-Marker. ✅
5. Sichere Diagnose-/Loeschfunktion in der WebUI bereitstellen. ✅
6. WLAN-Daten ueber die normale WebUI in NVS speichern, aber noch nicht automatisch aktivieren. ✅
7. Gespeicherte WLAN-Daten explizit aktivieren/deaktivieren und bei Verbindungsfehler automatisch auf das Standard-WLAN aus der Firmware zurueckfallen. ✅
8. Manuellen Setup-Access-Point `Waagen-Setup` bereitstellen. ✅
9. Minimal-Seite zum Speichern von SSID/Passwort bereitstellen. ✅
10. Wenn keine nutzbaren Daten vorhanden sind, SoftAP `Waagen-Setup` automatisch starten.
11. Neue WLAN-Daten erst nach erfolgreichem Verbindungstest als `active=true` markieren.
12. Erst wenn das stabil ist, `wifi_secrets.h` nur noch als Fallback/Entwicklungsoption verwenden.
13. Spaeter `wifi_secrets.h` ganz aus dem normalen Build entfernen.

## Migrationsstrategie

Fuer die Entwicklung sollte der Umbau vorsichtig erfolgen:

- `wifi_secrets.h` zunaechst als Fallback behalten.
- Gespeicherte WLAN-Daten nur verwenden, wenn sie explizit als aktiv/validiert markiert sind.
- Nicht aktive gespeicherte WLAN-Daten duerfen die Erreichbarkeit ueber das Standard-WLAN aus der Firmware nicht blockieren.
- Wenn keine aktiven gespeicherten WLAN-Daten vorhanden sind, optional noch das Standard-WLAN aus der Firmware nutzen oder Setup-AP starten.
- Erst nach erfolgreichen Tests die fest einkompilierten Secrets entfernen.

So bleibt das aktuell verbaute Geraet erreichbar und das Risiko eines OTA-Lockouts bleibt gering.
