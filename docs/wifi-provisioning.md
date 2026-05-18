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
Kaffeewaage-Setup
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
Netz: Kaffeewaage-Setup
Adresse: 192.168.4.1
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

Die WLAN-Daten sollen in NVS/Preferences gespeichert werden.

Aktuelle Keys im Modul `coffee_wifi`:

```text
Namespace: coffee_wifi
ssid
pass
active
```

Nach dem Phase-2a-Rueckschritt gilt: Eine nicht-leere SSID bedeutet nur, dass NVS-Daten vorhanden sind. Automatisch verwendet werden sie nur, wenn zusaetzlich `active=true` gesetzt ist. Der aktuelle sichere Entwicklungsstand setzt `active` beim Speichern bewusst nicht automatisch auf `true`.

Langfristig sollte diese Logik nicht in `main.cpp` wachsen, sondern in ein eigenes Modul ausgelagert werden, z.B.:

```text
src/coffee_wifi.h
src/coffee_wifi.cpp
```

Dieses Modul enthaelt in Phase 1/1b bereits:

- Laden der gespeicherten WLAN-Daten
- Fallback auf `wifi_secrets.h`
- Speichern neuer WLAN-Daten als vorbereitete, noch nicht automatisch aktive Daten
- Loeschen der WLAN-Daten
- Quelle der aktiven Zugangsdaten als Statusinformation
- Anzeige, ob NVS-Daten vorhanden sind
- Start der normalen WLAN-Verbindung

Phase 2b ergaenzt eine sichere WebUI-Speicherfunktion:

- SSID und Passwort koennen in NVS abgelegt werden.
- Der Key `active` bleibt dabei bewusst `false`.
- Die aktive Verbindung bleibt daher weiterhin bei `wifi_secrets.h`, solange keine spaetere Validierungslogik `active=true` setzt.
- Nach dem Speichern sollte die WebUI anzeigen: `NVS-Daten vorhanden: ja`, aber weiterhin `Quelle: wifi_secrets.h`.

Noch nicht enthalten:

- Start des Setup-Access-Points
- Webserver-Routen fuer Speichern/Aktivieren des Provisionings

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

Im HMI sollte es daher unter Einstellungen / System eine Funktion geben:

```text
WLAN einrichten
```

oder:

```text
Setup-WLAN starten
```

Damit kann der Nutzer das Geraet ohne erneutes Kompilieren in sein eigenes WLAN bringen.

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
4. Nach Phase-2a-Test: NVS-Daten nicht mehr ungeschuetzt automatisch aktivieren, sondern nur noch mit `active`-Marker. ✅
5. Sichere Diagnose-/Loeschfunktion in der WebUI bereitstellen. ✅
6. WLAN-Daten ueber die normale WebUI in NVS speichern, aber noch nicht automatisch aktivieren. ✅
7. Wenn keine nutzbaren Daten vorhanden sind, SoftAP `Kaffeewaage-Setup` starten.
8. Minimal-Seite zum Speichern von SSID/Passwort bereitstellen.
9. Neue WLAN-Daten erst nach erfolgreichem Verbindungstest als `active=true` markieren.
10. Erst wenn das stabil ist, `wifi_secrets.h` nur noch als Fallback/Entwicklungsoption verwenden.
11. Spaeter `wifi_secrets.h` ganz aus dem normalen Build entfernen.

## Migrationsstrategie

Fuer die Entwicklung sollte der Umbau vorsichtig erfolgen:

- `wifi_secrets.h` zunaechst als Fallback behalten.
- NVS-Daten nur verwenden, wenn sie explizit als aktiv/validiert markiert sind.
- Nicht aktive NVS-Daten duerfen die Erreichbarkeit ueber `wifi_secrets.h` nicht blockieren.
- Wenn keine aktiven NVS-Daten vorhanden sind, optional noch die alten Secrets nutzen oder Setup-AP starten.
- Erst nach erfolgreichen Tests die fest einkompilierten Secrets entfernen.

So bleibt das aktuell verbaute Geraet erreichbar und das Risiko eines OTA-Lockouts bleibt gering.
