# OTA-Dateinamen-Validierung

## Ziel

Die OTA-Seite soll verhindern, dass versehentlich das falsche Binary in den falschen Update-Bereich geschrieben wird. Das ist besonders wichtig, weil Firmware- und Dateisystem-Uploads beide ueber dieselbe WebUI erreichbar sind.

## Erlaubte Dateinamen

| Upload-Feld | erlaubte Datei | Zweck |
|---|---|---|
| Firmware | `firmware.bin` | Programm-Firmware |
| Dateisystem | `spiffs.bin` oder `littlefs.bin` | SPIFFS-/LittleFS-Webassets, insbesondere PWA-Icons |

## Clientseitige Pruefung

Die OTA-WebUI prueft die Dateinamen direkt bei der Auswahl und erneut beim Klick auf Upload. Bei falscher Datei wird der Upload blockiert und eine Klartextmeldung angezeigt.

Beispiele:

```text
Firmware: falsche Datei gewaehlt. Erwartet: firmware.bin.
SPIFFS: falsche Datei gewaehlt. Erwartet: spiffs.bin oder littlefs.bin.
```

## Serverseitige Pruefung

Der ESP prueft den Dateinamen zusaetzlich im Upload-Handler, bevor `Update.begin(...)` aufgerufen wird. Dadurch bleibt der Schutz auch dann wirksam, wenn ein Browserfehler, Cacheproblem oder ein manueller HTTP-Upload die clientseitige Pruefung umgehen wuerde.

Bei falschem Dateinamen antwortet der ESP mit HTTP 400 und startet keinen Flash-Vorgang.

## Neustartverhalten

Nach erfolgreichem Upload startet der ESP bewusst nicht automatisch neu. Die OTA-Seite zeigt `Upload erfolgreich. Neustart erforderlich.` und bietet einen Neustart-Button an.

## Testfaelle

Firmware-Feld:

1. `firmware.bin` auswaehlen -> Upload erlaubt.
2. `spiffs.bin` oder `littlefs.bin` auswaehlen -> Upload blockiert.

Dateisystem-Feld:

1. `spiffs.bin` oder `littlefs.bin` auswaehlen -> Upload erlaubt.
2. `firmware.bin` auswaehlen -> Upload blockiert.

Nach Firmware- oder Dateisystem-Upload:

1. Erfolgsmeldung abwarten.
2. Neustart per Button ausloesen.
3. WebUI neu laden.
4. Bei Dateisystem-Upload PWA-/Icon-URLs pruefen.
