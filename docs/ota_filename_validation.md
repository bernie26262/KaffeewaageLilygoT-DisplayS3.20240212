# OTA-Dateinamenprüfung

## Ziel

Die OTA-Seite soll verhindern, dass versehentlich ein falsches Binary in den falschen Flash-Bereich geschrieben wird.

## Erlaubte Dateinamen

| Upload-Feld | erlaubte Datei |
|---|---|
| Firmware | `firmware.bin` |
| Dateisystem | `spiffs.bin` oder `littlefs.bin` |

## Schutzebenen

Die Prüfung erfolgt doppelt:

1. clientseitig in der OTA-WebUI vor dem Upload
2. serverseitig im ESP-Upload-Handler vor `Update.begin(...)`

Dadurch bleibt der Schutz auch dann erhalten, wenn ein Upload nicht über die normale WebUI ausgelöst wird.

## Erwartete Meldungen

Falsche Firmware-Datei:

```text
Firmware: falsche Datei gewählt. Erwartet: firmware.bin.
```

Falsche Dateisystem-Datei:

```text
SPIFFS: falsche Datei gewählt. Erwartet: spiffs.bin oder littlefs.bin.
```

Nach erfolgreichem Upload:

```text
Upload erfolgreich. Neustart erforderlich.
```

Der Neustart erfolgt bewusst per Button und nicht automatisch.

## Testfälle

1. `firmware.bin` im Firmware-Feld hochladen: muss akzeptiert werden.
2. `spiffs.bin` oder `littlefs.bin` im Dateisystem-Feld hochladen: muss akzeptiert werden.
3. `spiffs.bin` im Firmware-Feld auswählen: muss blockiert werden.
4. `firmware.bin` im Dateisystem-Feld auswählen: muss blockiert werden.
5. Upload-Handler direkt mit falschem Dateinamen ansprechen: ESP muss HTTP 400 liefern.
