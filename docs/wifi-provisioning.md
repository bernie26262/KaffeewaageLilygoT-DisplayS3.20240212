# WLAN-Provisioning und Recovery – T4-S3 (Stand 2026-10-12)

## Geltungsbereich

Diese Datei beschreibt die **produktive T4-S3-/LVGL-Firmware** (`feature/t4s3-lvgl-touch`, Stand `5307cf2`). Frühere Entwürfe mit einem einzelnen Profil, `active`-Marker und einem Rückfall auf `wifi_secrets.h` sind **nicht mehr aktuell**. Der Legacy-Branch hat eine eigene Implementierung; dort nichts allein aufgrund dieser Datei umstellen.

Die Konfiguration erfolgt über **Einstellungen → WLAN** in der normalen WebUI. Auf dem T4-S3-HMI ist **WLAN / Netzwerk** eine kompakte Statusseite mit Setup-AP-Bedienung, keine Passwort-Eingabemaske.

## Zwei WLAN-Profile und Speicherung

Die Firmware speichert WLAN 1 und WLAN 2 dauerhaft im NVS-Namespace `coffee_wifi`:

| NVS-Key | Bedeutung |
|---|---|
| `schema` | Versionskennzeichen für die Profilstruktur (`1`) |
| `p1_ssid`, `p1_pass` | SSID und Passwort von WLAN 1 |
| `p2_ssid`, `p2_pass` | SSID und Passwort von WLAN 2 |
| `pref` | bevorzugtes Profil (`1` oder `2`) |
| `auto` | automatische Verbindung aktiviert |

Beim ersten Start mit dem neuen Schema werden vorhandene ältere Single-Profil-Werte (`ssid`, `pass`, `active`) bei Bedarf nach WLAN 1 übertragen. Die alten Keys werden **nicht gelöscht**, um ein Rollback zu ermöglichen.

- Ein leeres Passwortfeld beim Bearbeiten bedeutet: **bestehendes Passwort behalten**.
- Die WebUI liefert niemals den gespeicherten Passworttext, sondern nur `gespeichert` / `nicht gespeichert`.
- Die aktive Verbindung kann von der Preferred-Auswahl abweichen. Das Ändern von `pref` erzwingt **keinen sofortigen Verbindungswechsel**; es wirkt beim nächsten Verbindungszyklus oder Neustart.
- Es gibt **keinen versteckten Rückfall auf fest einkompilierte WLAN-Zugangsdaten**.

## Verbindung, Failover und automatisches Recovery

1. Beim Boot versucht die Waage zunächst das bevorzugte, eingerichtete Profil; Timeout ungefähr **15 Sekunden**.
2. Falls erfolglos, folgt das andere eingerichtete Profil (ebenfalls ungefähr **15 Sekunden**).
3. Wenn kein Profil verbindet, startet **`Waagen-Setup` im Modus `WIFI_AP` (AP-only)** mit `192.168.4.1`.
4. Solange der automatische Recovery-AP aktiv ist, laufen **keine konkurrierenden STA-Reconnects oder Scans**. Der AP und die Setup-Seite sind weiter erreichbar.
5. Nach einer erfolgreichen Verbindung wird nicht wegen der wieder auftauchenden Preferred-SSID sofort umgeschaltet. Bei späterem Abbruch beginnt der Zyklus mit dem **zuletzt funktionierenden Profil**, danach wird das andere versucht.
6. Den AP unter `http://192.168.4.1/` mit **„Setup-WLAN beenden / WLAN erneut versuchen“** oder am HMI beenden: Dadurch startet ein neuer gespeicherter Verbindungszyklus **ohne ESP32-Neustart**.

Wenn gar keine nutzbaren Profile eingerichtet bzw. automatische Verbindungen deaktiviert sind, führt der Verbindungszyklus ebenfalls in den Recovery-AP. Bei einem fehlgeschlagenen AP-Start erfolgt ein zeitlich begrenzter neuer Startversuch (Code: 30 Sekunden), keine enge Endlosschleife.

## Manueller Setup-AP

Ein manuell in WebUI oder HMI gestarteter `Waagen-Setup`-AP läuft dagegen bewusst mit **`WIFI_AP_STA`** neben der bestehenden WLAN-Verbindung. So bleibt die normale Heimnetz-WebUI erreichbar. „Setup-WLAN beenden“ schaltet den manuellen AP wieder ab. Im Gegensatz zum automatischen Recovery löst dies keinen neuen Profil-Failover-Zyklus aus.

**Stabilitätsdetail des ESP32-S3:** Nach `WiFi.mode(WIFI_AP_STA)` und beim Start/Stop des Setup-AP **nicht erneut `WiFi.setSleep(false)` aufrufen**. Das führte früher zu Panic/Exceptions. Der normale WLAN-Start darf den Sleep-Modus einmalig konfigurieren.

Die separate Setup-Seite über `192.168.4.1` bietet WLAN1-Eingabe, Speichern/Verwenden, Neustart und den Retry-/Stop-Button. Sie ist **kein Captive Portal** und bietet **nicht die vollständige normale WebUI**; diese Erweiterung wurde bewusst zurückgestellt.

## Normale WebUI und API

Die normale WebUI zeigt getrennt: tatsächlich verbundene SSID, IP-Adresse, RSSI, Preferred-Profil sowie die beiden Profilkarten mit Einrichten/Bearbeiten, Löschen und „Als bevorzugt verwenden“.

| HTTP-Endpunkt | Aufgabe |
|---|---|
| `POST /api/wifi/profile` | Profil 1/2 mit SSID und optionalem Passwort speichern |
| `DELETE /api/wifi/profile?profile=1` (oder `2`) | Profil löschen |
| `POST /api/wifi/preferred` | bevorzugtes Profil setzen |
| `POST /api/wifi/setup-ap/start` | manuellen Setup-AP starten |
| `POST /api/wifi/setup-ap/stop` | AP beenden; in automatischer Recovery erneut verbinden |
| `POST /api/wifi/setup` | Eingabe auf der separaten Setup-Seite (`192.168.4.1`) |

Die POST-Felder für `/api/wifi/profile` sind `profile`, `ssid`, optional `password`; für `/api/wifi/preferred` ist es `profile` (Formulardaten). Der WebSocket-State enthält `wifi_profile1_*`, `wifi_profile2_*`, `wifi_preferred_profile`, `wifi_automatic_connection`, `wifi_recovery_mode` und die aktive `wifi_ssid`. Passwortstatus-Felder sind boolesch, **keine Passwörter**.

## Hardwaretests der Portierung

Auf der T4-S3 mit realer FritzBox und zweitem WLAN (`AndroidAP2`) bestätigt:

- WLAN2 speichern; WLAN1 bleibt bevorzugt und verbunden.
- Preferred auf WLAN2 setzen: bestehende Verbindung bleibt; Boot verbindet WLAN2.
- Failover bei unerreichbarem WLAN1 nach Timeout auf WLAN2; Verbindung bleibt stabil.
- Nach Ausfall auch von WLAN2 startet der automatische AP-only-Recovery-AP.
- Smartphone kann `Waagen-Setup` verbinden und `192.168.4.1` aufrufen; AP läuft stabil.
- Retry über HMI und Recovery-WebUI verbindet nach Wiederverfügbarkeit ohne ESP-Neustart.
- HMI zeigt im AP-only-Recovery keine falsche vorherige Ziel-SSID.

Die vollständige normale WebUI über Recovery-AP bleibt ein **offenes, ausdrücklich zurückgestelltes Thema**.

## Sichere Prüfung ohne Abschalten des Heim-WLANs

Für Failover-/Recovery-Tests bei weiterhin laufendem Heimnetz kann die SSID von WLAN1 **vorübergehend** auf einen nicht vorhandenen Testnamen gesetzt werden; das Passwortfeld bleibt leer (das echte Passwort bleibt gespeichert). WLAN2 muss zunächst erreichbar sein. Nach Neustart verbindet die Waage WLAN2. Wird danach WLAN2 abgeschaltet, muss `Waagen-Setup` erscheinen. Nach dem Test WLAN2 wieder einschalten, Retry auslösen und **unbedingt die ursprüngliche WLAN1-SSID wiederherstellen**.

Weitere Regressionstests: `docs/test-plan.md`.
