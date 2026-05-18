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

Moegliche Keys:

```text
wifiSsid
wifiPass
wifiConfigured
```

Langfristig sollte diese Logik nicht in `main.cpp` wachsen, sondern in ein eigenes Modul ausgelagert werden, z.B.:

```text
src/coffee_wifi.h
src/coffee_wifi.cpp
```

Dieses Modul koennte enthalten:

- Laden der gespeicherten WLAN-Daten
- Speichern neuer WLAN-Daten
- Loeschen der WLAN-Daten
- Start der normalen WLAN-Verbindung
- Start des Setup-Access-Points
- Webserver-Routen fuer das Provisioning

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

1. Neues Modul `coffee_wifi` anlegen.
2. WLAN-Zugangsdaten aus NVS laden.
3. Wenn keine Daten vorhanden sind, SoftAP `Kaffeewaage-Setup` starten.
4. Minimal-Seite zum Speichern von SSID/Passwort bereitstellen.
5. Nach Speichern neu starten oder WLAN-Verbindung neu aufbauen.
6. Erst wenn das stabil ist, `wifi_secrets.h` nur noch als Fallback/Entwicklungsoption verwenden.
7. Spaeter `wifi_secrets.h` ganz aus dem normalen Build entfernen.

## Migrationsstrategie

Fuer die Entwicklung sollte der Umbau vorsichtig erfolgen:

- `wifi_secrets.h` zunaechst als Fallback behalten.
- Wenn NVS-Daten vorhanden sind, diese bevorzugen.
- Wenn keine NVS-Daten vorhanden sind, optional noch die alten Secrets nutzen oder Setup-AP starten.
- Erst nach erfolgreichen Tests die fest einkompilierten Secrets entfernen.

So bleibt das aktuell verbaute Geraet erreichbar und das Risiko eines OTA-Lockouts bleibt gering.
