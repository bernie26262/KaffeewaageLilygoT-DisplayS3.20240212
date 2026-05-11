#include "coffee_ota.h"

#include <Arduino.h>
#include <SPIFFS.h>
#include <Update.h>

static bool otaRebootRequested = false;
static unsigned long otaRebootRequestMs = 0;

static const char OTA_UPDATE_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="de">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Kaffeewaage OTA Update</title>
  <style>
    body { font-family: Arial, sans-serif; background: #f3f6fb; margin: 0; padding: 24px; color: #1f2933; }
    .panel { max-width: 720px; margin: 0 auto; background: white; border-radius: 12px; padding: 22px; box-shadow: 0 4px 18px rgba(0,0,0,0.12); }
    h1 { margin-top: 0; }
    .card { border: 1px solid #d7dde8; border-radius: 10px; padding: 16px; margin: 16px 0; background: #fbfcff; }
    .hint { color: #52606d; font-size: 0.95rem; }
    input[type=file] { display: block; margin: 12px 0; width: 100%; }
    button, .button { display: inline-block; border: 0; border-radius: 8px; padding: 10px 16px; background: #2563eb; color: white; font-weight: bold; text-decoration: none; cursor: pointer; }
    button:hover, .button:hover { background: #1d4ed8; }
    button:disabled { background: #9aa5b1; cursor: wait; }
    .danger { background: #b91c1c; }
    .danger:hover { background: #991b1b; }
    code { background: #edf2f7; padding: 2px 5px; border-radius: 4px; }
    progress { width: 100%; height: 22px; margin-top: 12px; }
    .status { min-height: 1.4em; margin-top: 10px; font-weight: bold; }
    .reboot-panel { display: none; border: 1px solid #bbf7d0; border-radius: 10px; padding: 16px; margin: 16px 0; background: #f0fdf4; }
    .reboot-panel.visible { display: block; }
    .ok { color: #047857; }
    .err { color: #b91c1c; }
    .busy { color: #1d4ed8; }
  </style>
</head>
<body>
  <div class="panel">
    <h1>Kaffeewaage OTA Update</h1>
    <p class="hint">Hier koennen Firmware und SPIFFS-Dateisystem getrennt aktualisiert werden.</p>

    <div class="card">
      <h2>Firmware aktualisieren</h2>
      <p class="hint">Datei: <code>.pio/build/KaffeewaageLilygoT-DisplayS3_20240212/firmware.bin</code></p>
      <form class="ota-form" method="POST" action="/update/firmware" enctype="multipart/form-data" data-label="Firmware">
        <input type="file" name="update" accept=".bin" required>
        <button class="danger" type="submit">Firmware hochladen</button>
        <progress value="0" max="100" hidden></progress>
        <div class="status"></div>
      </form>
    </div>

    <div class="card">
      <h2>Dateisystem aktualisieren</h2>
      <p class="hint">Datei: <code>.pio/build/KaffeewaageLilygoT-DisplayS3_20240212/spiffs.bin</code></p>
      <form class="ota-form" method="POST" action="/update/filesystem" enctype="multipart/form-data" data-label="SPIFFS">
        <input type="file" name="update" accept=".bin" required>
        <button type="submit">SPIFFS hochladen</button>
        <progress value="0" max="100" hidden></progress>
        <div class="status"></div>
      </form>
    </div>

    <div id="reboot-panel" class="reboot-panel">
      <h2>Neustart erforderlich</h2>
      <p class="hint">Das Update wurde erfolgreich eingespielt. Bitte starte den ESP32 jetzt neu, damit die neue Firmware bzw. das neue Dateisystem aktiv wird.</p>
      <button id="reboot-button" class="danger" type="button">ESP32 jetzt neu starten</button>
      <div id="reboot-status" class="status"></div>
    </div>

    <p><a class="button" href="/">Zurueck zur WebUI</a></p>
  </div>

  <script>
    function setStatus(el, text, cls) {
      el.textContent = text;
      el.className = 'status ' + (cls || '');
    }

    var rebootButton = document.getElementById('reboot-button');
    var rebootStatus = document.getElementById('reboot-status');
    if (rebootButton) {
      rebootButton.addEventListener('click', function() {
        rebootButton.disabled = true;
        setStatus(rebootStatus, 'Neustart wird angefordert...', 'busy');

        var xhr = new XMLHttpRequest();
        xhr.onload = function() {
          if (xhr.status >= 200 && xhr.status < 300) {
            setStatus(rebootStatus, 'Neustart angefordert. Verbindung bricht gleich ab; die WebUI danach neu laden.', 'ok');
          } else {
            rebootButton.disabled = false;
            setStatus(rebootStatus, 'Neustart fehlgeschlagen: HTTP ' + xhr.status, 'err');
          }
        };
        xhr.onerror = function() {
          rebootButton.disabled = false;
          setStatus(rebootStatus, 'Neustart fehlgeschlagen.', 'err');
        };
        xhr.open('POST', '/update/reboot');
        xhr.send();
      });
    }

    document.querySelectorAll('.ota-form').forEach(function(form) {
      form.addEventListener('submit', function(e) {
        e.preventDefault();

        var fileInput = form.querySelector('input[type=file]');
        var button = form.querySelector('button');
        var progress = form.querySelector('progress');
        var status = form.querySelector('.status');
        var label = form.dataset.label || 'Update';

        if (!fileInput.files.length) {
          setStatus(status, 'Bitte zuerst eine Datei auswählen.', 'err');
          return;
        }

        var xhr = new XMLHttpRequest();
        var data = new FormData(form);

        button.disabled = true;
        progress.hidden = false;
        progress.value = 0;
        setStatus(status, label + '-Upload startet...', 'busy');

        xhr.upload.onprogress = function(event) {
          if (event.lengthComputable) {
            var percent = Math.round((event.loaded / event.total) * 100);
            progress.value = percent;
            setStatus(status, label + '-Upload: ' + percent + ' %', 'busy');
          }
        };

        xhr.onload = function() {
          progress.value = 100;
          button.disabled = false;
          if (xhr.status >= 200 && xhr.status < 300) {
            setStatus(status, label + '-Update erfolgreich eingespielt. Bitte ESP32 neu starten.', 'ok');
            var panel = document.getElementById('reboot-panel');
            if (panel) {
              panel.classList.add('visible');
              panel.scrollIntoView({ behavior: 'smooth', block: 'nearest' });
            }
          } else {
            setStatus(status, label + '-Update fehlgeschlagen: HTTP ' + xhr.status, 'err');
          }
        };

        xhr.onerror = function() {
          button.disabled = false;
          setStatus(status, label + '-Update fehlgeschlagen: Verbindung abgebrochen.', 'err');
        };

        xhr.ontimeout = function() {
          button.disabled = false;
          setStatus(status, label + '-Update fehlgeschlagen: Timeout.', 'err');
        };

        xhr.open('POST', form.action);
        xhr.send(data);
      });
    });
  </script>
</body>
</html>
)rawliteral";

static void onUpdateRequest(AsyncWebServerRequest *request) {
  request->send_P(200, "text/html", OTA_UPDATE_PAGE);
}

static void onUpdateFinished(AsyncWebServerRequest *request) {
  const bool ok = !Update.hasError();
  request->send(200, "text/plain", ok
    ? "Update erfolgreich eingespielt. Neustart erforderlich."
    : "Update fehlgeschlagen. Details siehe serieller Monitor.");
}

static void onUpdateRebootRequest(AsyncWebServerRequest *request) {
  AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "Neustart angefordert.");
  response->addHeader("Connection", "close");
  request->send(response);
  coffeeOtaRequestReboot();
}

static void handleFirmwareUpload(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
  (void)request;
  if (index == 0) {
    Serial.print("Firmware update started: ");
    Serial.println(filename);
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
      Update.printError(Serial);
    }
  }

  if (!Update.hasError()) {
    if (Update.write(data, len) != len) {
      Update.printError(Serial);
    }
  }

  if (final) {
    if (Update.end(true)) {
      Serial.print("Firmware update complete: ");
      Serial.println(index + len);
    } else {
      Update.printError(Serial);
    }
  }
}

static void handleFilesystemUpload(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
  (void)request;
  if (index == 0) {
    Serial.print("SPIFFS update started: ");
    Serial.println(filename);
    SPIFFS.end();
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_SPIFFS)) {
      Update.printError(Serial);
    }
  }

  if (!Update.hasError()) {
    if (Update.write(data, len) != len) {
      Update.printError(Serial);
    }
  }

  if (final) {
    if (Update.end(true)) {
      Serial.print("SPIFFS update complete: ");
      Serial.println(index + len);
    } else {
      Update.printError(Serial);
    }
  }
}

void coffeeOtaBegin(AsyncWebServer& server) {
  server.on("/update", HTTP_GET, onUpdateRequest);
  server.on("/update/firmware", HTTP_POST, onUpdateFinished, handleFirmwareUpload);
  server.on("/update/filesystem", HTTP_POST, onUpdateFinished, handleFilesystemUpload);
  server.on("/update/reboot", HTTP_POST, onUpdateRebootRequest);
}

void coffeeOtaRequestReboot() {
  otaRebootRequested = true;
  otaRebootRequestMs = millis();
}

void coffeeOtaLoop() {
  if (otaRebootRequested && (millis() - otaRebootRequestMs > 1000)) {
    Serial.println("Restarting ESP32 after OTA request...");
    ESP.restart();
  }
}
