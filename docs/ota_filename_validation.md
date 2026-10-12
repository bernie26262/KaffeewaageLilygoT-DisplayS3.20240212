# OTA-Dateinamenprüfung und Upload-Sperre – T4-S3

## Zweck

Die T4-S3 bietet **nur Firmware-OTA** unter `http://<ip>/update`. Zulässiger Dateiname: `firmware.bin` (aus `.pio\build\lilygo-t4-s3-lvgl\`). `spiffs.bin`, `littlefs.bin`, `bootloader.bin` und sonstige Dateien dürfen **nicht** als Firmware hochgeladen werden. Ein Dateisystem-Upload ist nicht mehr vorhanden.

## Schutzebenen

1. **Browser:** Prüfung des ausgewählten Dateinamens und erneute Validierung beim Upload; falsche Datei blockiert den Upload-Button. Bei gültiger Datei ist dieser grün und aktiv.
2. **ESP:** Prüfung im Handler vor `Update.begin(...)`. Falscher Name ergibt HTTP 400. Die BLE-Abschaltung wird erst nach einer gültigen Dateinamenprüfung ausgelöst.
3. **Nach Erfolg:** Erst `Update.end(true)` setzt den Reboot-Pflichtstatus. `GET /update/status` liefert `{"reboot_required":true}`; ein weiterer Upload vor Neustart ergibt HTTP 409, auch ohne Browser-UI.
4. **Abbruch/Fehler:** Keine Erfolgsmeldung bei unterbrochenem Update. Ein späterer Versuch verwirft mit `Update.abort()` gegebenenfalls einen laufenden unvollständigen Update-Zustand und beginnt bei 0 %. Ein Byte-Resume gibt es nicht.

## Sichtbare Meldungen

Bei falscher Firmware-Datei beispielsweise:

```text
Firmware: falsche Datei gewaehlt. Erwartet: firmware.bin.
```

Beim laufenden Upload werden Fortschritt und Warnung angezeigt; nach Erfolg erscheint der Neustartbereich. Während eines erfolgreichen Updates bleiben Navigation und Dateiauswahl gesperrt. Nach **Neustartanforderung** erscheint „Zurück zur WebUI“ wieder; nach abgeschlossenem Boot steht ein neuer OTA-Upload wieder zur Verfügung.

## Testfälle / Validierungsstand

- `firmware.bin` wird angenommen; Firmware-OTA mehrfach auf Hardware bestätigt.
- `bootloader.bin` wird in der Oberfläche abgewiesen; auf Hardware bestätigt.
- Nach erfolgreichem Upload und `Strg+F5`: Upload weiter gesperrt; auf Hardware bestätigt.
- Neustart über OTA-Seite: funktioniert; BLE danach wieder verfügbar; auf Hardware bestätigt.
- Browserseitiges Neuversuchen nach Fehler bzw. Abbruch: im Code vorgesehen und simuliert; **absichtlich abgebrochener Firmware-Upload noch nicht auf Hardware getestet**.

Weitere Abläufe in `docs/webui-pwa-ota.md` und `docs/test-plan.md`.
