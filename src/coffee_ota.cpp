#include "coffee_ota.h"
#include "ble_scale.h"

#include <Arduino.h>
#include <Update.h>

static bool otaRebootRequested = false;
static unsigned long otaRebootRequestMs = 0;
static bool otaUploadRejected = false;
static bool otaUploadCompletedSuccessfully = false;
static String otaUploadRejectReason;

static const char OTA_UPDATE_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="de">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <meta name="theme-color" content="#061018">
  <link rel="icon" href="/favicon.ico">
  <title>Kaffeewaage OTA Update</title>
  <style>
    :root {
      font-family: system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
      color: #f4f7fb;
      background: #061018;
      --bg: #061018;
      --card: #101b27;
      --border: rgba(148, 163, 184, .24);
      --border-strong: rgba(148, 163, 184, .38);
      --text: #f4f7fb;
      --muted: #9aa8b8;
      --muted-2: #c4ccd8;
      --accent: #55c342;
      --accent-2: #3fa72d;
      --accent-soft: rgba(85, 195, 66, .18);
      --danger: #ef4444;
      --warn: #f59e0b;
      --shadow: 0 18px 48px rgba(0,0,0,.32);
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      min-height: 100vh;
      padding: 18px;
      background:
        radial-gradient(circle at 14% 0%, rgba(85,195,66,.14), transparent 28%),
        linear-gradient(180deg, #0b111b 0%, var(--bg) 48%, #040a10 100%);
    }
    main { max-width: 760px; margin: 0 auto; display: grid; gap: 14px; }
    .card {
      background: linear-gradient(180deg, rgba(20,34,53,.96), rgba(13,24,37,.96));
      border: 1px solid var(--border);
      border-radius: 22px;
      padding: 18px;
      box-shadow: var(--shadow);
    }
    .top { display: flex; justify-content: space-between; align-items: center; gap: 10px; padding: 12px 14px; }
    h1 { margin: 0; font-size: 1.15rem; letter-spacing: -.02em; }
    h2 { margin: 0 0 8px; font-size: 1.05rem; }
    .pill {
      flex: 0 0 auto;
      padding: 6px 10px;
      border-radius: 999px;
      background: var(--accent-soft);
      color: #bbf7d0;
      border: 1px solid rgba(85,195,66,.36);
      font-size: .82rem;
      white-space: nowrap;
    }
    .hint { margin: 0; color: var(--muted); font-size: .9rem; line-height: 1.45; }
    .file-path { margin-top: 12px; }
    code {
      display: inline-block;
      max-width: 100%;
      padding: 4px 7px;
      border-radius: 8px;
      background: #0b1520;
      color: var(--muted-2);
      overflow-wrap: anywhere;
    }
    input[type=file] {
      display: block;
      width: 100%;
      margin: 16px 0 8px;
      padding: 11px;
      border: 1px solid var(--border-strong);
      border-radius: 12px;
      background: #0b1520;
      color: var(--text);
      font: inherit;
    }
    input[type=file]::file-selector-button {
      margin-right: 10px;
      border: 1px solid rgba(85,195,66,.46);
      border-radius: 10px;
      padding: 8px 11px;
      background: #1a2533;
      color: var(--text);
      font-weight: 700;
      cursor: pointer;
    }
    button, .button {
      width: 100%;
      display: inline-flex;
      align-items: center;
      justify-content: center;
      border: 1px solid rgba(85,195,66,.36);
      border-radius: 16px;
      padding: 14px 16px;
      font: inherit;
      font-size: 1rem;
      font-weight: 750;
      background: linear-gradient(180deg, var(--accent), var(--accent-2));
      color: #f8fff8;
      text-decoration: none;
      cursor: pointer;
      box-shadow: 0 12px 30px rgba(63,167,45,.20);
    }
    button.secondary, .button.secondary {
      background: #1a2533;
      color: var(--text);
      border-color: rgba(85,195,66,.46);
      box-shadow: none;
    }
    button.danger { background: rgba(239,68,68,.18); color: #fecaca; border-color: rgba(239,68,68,.55); box-shadow: none; }
    button:disabled { background: #334155; color: #94a3b8; border-color: rgba(148,163,184,.20); cursor: not-allowed; box-shadow: none; }
    button:focus-visible, .button:focus-visible, input:focus-visible { outline: 2px solid rgba(85,195,66,.55); outline-offset: 2px; }
    .actions { display: grid; gap: 10px; margin-top: 14px; }
    progress {
      width: 100%;
      height: 18px;
      margin-top: 14px;
      accent-color: var(--accent);
    }
    .status, .file-check { min-height: 1.35em; margin-top: 10px; font-size: .9rem; line-height: 1.4; }
    .status { font-weight: 750; }
    .file-check { color: var(--muted-2); }
    .ok { color: #86efac; }
    .err { color: #fca5a5; }
    .busy { color: #fde68a; }
    .upload-warning {
      display: none;
      gap: 7px;
      margin-top: 14px;
      padding: 13px 14px;
      border: 1px solid rgba(245,158,11,.48);
      border-radius: 14px;
      background: rgba(245,158,11,.13);
      color: #fde68a;
      line-height: 1.4;
    }
    .upload-warning.visible { display: grid; }
    .upload-warning strong { color: #fef3c7; }
    .reboot-panel {
      display: none;
      border-color: rgba(85,195,66,.42);
      background: linear-gradient(180deg, rgba(20,45,36,.96), rgba(11,31,22,.94));
    }
    .reboot-panel.visible { display: block; }
    .reboot-panel h2 { color: #bbf7d0; }
    .footer-actions { display: grid; gap: 10px; }
    @media (min-width: 560px) {
      .actions, .footer-actions { grid-template-columns: repeat(2, minmax(0, 1fr)); }
      .actions button:only-child, .footer-actions > :only-child { grid-column: 1 / -1; }
    }
  </style>
</head>
<body>
<main>
  <section class="card top">
    <h1>Kaffeewaage</h1>
    <span class="pill">OTA Update</span>
  </section>

  <section class="card">
    <h2>Firmware aktualisieren</h2>
    <p class="hint file-path">Datei: <code>.pio/build/KaffeewaageLilygoT-DisplayS3_20240212/firmware.bin</code></p>

    <form id="firmware-form" class="ota-form" method="POST" action="/update/firmware" enctype="multipart/form-data" data-label="Firmware" data-expected="firmware.bin">
      <input type="file" name="update" accept=".bin" required>
      <div class="file-check" aria-live="polite"></div>
      <div class="actions">
        <button class="danger" type="submit">Firmware hochladen</button>
      </div>
      <progress value="0" max="100" hidden></progress>
      <div class="status" role="status" aria-live="polite"></div>
    </form>

    <div id="upload-warning" class="upload-warning" role="alert" aria-live="assertive">
      <strong>Firmware-Update laeuft.</strong>
      <span>Diese Seite nicht verlassen, das WLAN nicht wechseln und die Waage nicht ausschalten.</span>
    </div>

  </section>

  <section id="reboot-panel" class="card reboot-panel">
    <h2>Update erfolgreich</h2>
    <p class="hint">Die neue Firmware wurde vollstaendig eingespielt. Starte den ESP32 jetzt neu, damit sie aktiv wird.</p>
    <div class="actions">
      <button id="reboot-button" type="button">ESP32 jetzt neu starten</button>
    </div>
    <div id="reboot-status" class="status" role="status" aria-live="polite"></div>
  </section>

  <section class="footer-actions">
    <a id="back-link" class="button secondary" href="/">Zurueck zur WebUI</a>
  </section>
</main>

<script>
  var uploadInProgress = false;

  function setStatus(el, text, cls) {
    el.textContent = text;
    el.className = 'status ' + (cls || '');
  }

  function setFileCheck(el, text, cls) {
    if (!el) return;
    el.textContent = text;
    el.className = 'file-check ' + (cls || '');
  }

  function expectedNamesFor(form) {
    return (form.dataset.expected || '')
      .split(',')
      .map(function(name) { return name.trim().toLowerCase(); })
      .filter(Boolean);
  }

  function validateSelectedFile(form, fileInput, fileCheck) {
    var names = expectedNamesFor(form);
    var file = fileInput.files && fileInput.files[0];
    var label = form.dataset.label || 'Update';

    if (!file) {
      setFileCheck(fileCheck, '', '');
      return false;
    }

    var selected = (file.name || '').toLowerCase();
    if (names.indexOf(selected) === -1) {
      setFileCheck(fileCheck, label + ': falsche Datei gewaehlt. Erwartet: ' + names.join(' oder ') + '.', 'err');
      return false;
    }

    setFileCheck(fileCheck, label + ': Datei passt (' + file.name + ').', 'ok');
    return true;
  }

  function setUploadActive(active) {
    uploadInProgress = !!active;
    var warning = document.getElementById('upload-warning');
    var backLink = document.getElementById('back-link');
    if (warning) warning.classList.toggle('visible', uploadInProgress);
    if (backLink) {
      backLink.setAttribute('aria-disabled', uploadInProgress ? 'true' : 'false');
      backLink.style.pointerEvents = uploadInProgress ? 'none' : '';
      backLink.style.opacity = uploadInProgress ? '.45' : '';
    }
  }

  window.addEventListener('beforeunload', function(event) {
    if (!uploadInProgress) return;
    event.preventDefault();
    event.returnValue = '';
  });

  var rebootButton = document.getElementById('reboot-button');
  var rebootStatus = document.getElementById('reboot-status');
  if (rebootButton) {
    rebootButton.addEventListener('click', function() {
      rebootButton.disabled = true;
      setStatus(rebootStatus, 'Neustart wird angefordert...', 'busy');

      var xhr = new XMLHttpRequest();
      xhr.onload = function() {
        if (xhr.status >= 200 && xhr.status < 300) {
          setStatus(rebootStatus, 'Neustart angefordert. Die Verbindung bricht gleich ab; die WebUI danach neu laden.', 'ok');
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
    var fileInput = form.querySelector('input[type=file]');
    var button = form.querySelector('button');
    var progress = form.querySelector('progress');
    var status = form.querySelector('.status');
    var fileCheck = form.querySelector('.file-check');
    var label = form.dataset.label || 'Update';

    fileInput.addEventListener('change', function() {
      validateSelectedFile(form, fileInput, fileCheck);
    });

    form.addEventListener('submit', function(e) {
      e.preventDefault();

      if (!fileInput.files.length) {
        setStatus(status, 'Bitte zuerst eine .bin-Datei auswaehlen.', 'err');
        setFileCheck(fileCheck, '', '');
        return;
      }

      if (!validateSelectedFile(form, fileInput, fileCheck)) {
        setStatus(status, label + '-Upload blockiert: falsche Datei ausgewaehlt.', 'err');
        return;
      }

      var xhr = new XMLHttpRequest();
      var data = new FormData(form);

      button.disabled = true;
      fileInput.disabled = true;
      progress.hidden = false;
      progress.value = 0;
      setUploadActive(true);
      setStatus(status, label + '-Upload startet...', 'busy');

      xhr.upload.onprogress = function(event) {
        if (event.lengthComputable) {
          var percent = Math.round((event.loaded / event.total) * 100);
          progress.value = percent;
          setStatus(status, label + '-Upload: ' + percent + ' %', 'busy');
        }
      };

      function finishUploadUi() {
        setUploadActive(false);
        button.disabled = false;
        fileInput.disabled = false;
      }

      xhr.onload = function() {
        finishUploadUi();
        if (xhr.status >= 200 && xhr.status < 300) {
          progress.value = 100;
          setStatus(status, label + '-Update erfolgreich eingespielt.', 'ok');
          var panel = document.getElementById('reboot-panel');
          if (panel) {
            panel.classList.add('visible');
            panel.scrollIntoView({ behavior: 'smooth', block: 'nearest' });
          }
        } else {
          setStatus(status, label + '-Update fehlgeschlagen: HTTP ' + xhr.status + '. Ein Fortsetzen an der Abbruchstelle ist nicht moeglich; firmware.bin bitte erneut vollstaendig hochladen.', 'err');
        }
      };

      xhr.onerror = function() {
        finishUploadUi();
        setStatus(status, label + '-Update unterbrochen. Ein Fortsetzen an der Abbruchstelle ist nicht moeglich; firmware.bin bitte erneut vollstaendig hochladen.', 'err');
      };

      xhr.ontimeout = function() {
        finishUploadUi();
        setStatus(status, label + '-Update unterbrochen: Timeout. Ein Fortsetzen an der Abbruchstelle ist nicht moeglich; firmware.bin bitte erneut vollstaendig hochladen.', 'err');
      };

      xhr.onabort = function() {
        finishUploadUi();
        setStatus(status, label + '-Update unterbrochen. Ein Fortsetzen an der Abbruchstelle ist nicht moeglich; firmware.bin bitte erneut vollstaendig hochladen.', 'err');
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
  if (otaUploadRejected) {
    const String message = otaUploadRejectReason.length()
      ? otaUploadRejectReason
      : String("Update abgelehnt: falscher Dateiname.");
    otaUploadRejected = false;
    otaUploadRejectReason = String();

    AsyncWebServerResponse *response = request->beginResponse(400, "text/plain", message);
    response->addHeader("Connection", "close");
    request->send(response);
    return;
  }

  const bool ok = otaUploadCompletedSuccessfully && !Update.hasError();
  otaUploadCompletedSuccessfully = false;
  AsyncWebServerResponse *response = request->beginResponse(ok ? 200 : 500, "text/plain", ok
    ? "Update erfolgreich eingespielt. Neustart erforderlich."
    : "Update fehlgeschlagen oder unterbrochen. Firmware erneut vollstaendig hochladen.");
  response->addHeader("Connection", "close");
  request->send(response);
}

static void onUpdateRebootRequest(AsyncWebServerRequest *request) {
  AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "Neustart angefordert.");
  response->addHeader("Connection", "close");
  request->send(response);
  coffeeOtaRequestReboot();
}

static bool isExpectedOtaFilename(const String& filename, const char* expected) {
  String lower = filename;
  lower.toLowerCase();
  return lower == expected;
}

static void rejectOtaUpload(const String& filename, const String& expected) {
  otaUploadRejected = true;
  otaUploadRejectReason = String("Update abgelehnt: falsche Datei '") + filename + "'. Erwartet: " + expected + ".";
  Serial.println(otaUploadRejectReason);
}

static void handleFirmwareUpload(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
  (void)request;
  if (index == 0) {
    otaUploadRejected = false;
    otaUploadCompletedSuccessfully = false;
    otaUploadRejectReason = String();

    // Ein abgebrochener Browser-Upload kann den Updater im laufenden Zustand
    // hinterlassen. Vor einem neuen Upload diesen Zustand sauber verwerfen;
    // das neue Firmware-Image wird danach immer wieder bei 0 % begonnen.
    if (Update.isRunning()) {
      Serial.println("Aborting incomplete previous firmware update before retry...");
      Update.abort();
    }

    if (!isExpectedOtaFilename(filename, "firmware.bin")) {
      rejectOtaUpload(filename, "firmware.bin");
      return;
    }

    coffeeBleScaleStopForOta();

    Serial.print("Firmware update started: ");
    Serial.println(filename);
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
      Update.printError(Serial);
    }
  }

  if (otaUploadRejected) {
    return;
  }

  if (!Update.hasError()) {
    if (Update.write(data, len) != len) {
      Update.printError(Serial);
    }
  }

  if (final) {
    otaUploadCompletedSuccessfully = Update.end(true);
    if (otaUploadCompletedSuccessfully) {
      Serial.print("Firmware update complete: ");
      Serial.println(index + len);
    } else {
      Update.printError(Serial);
    }
  }
}

void coffeeOtaBegin(AsyncWebServer& server) {
  server.on("/update", HTTP_GET, onUpdateRequest);
  server.on("/update/firmware", HTTP_POST, onUpdateFinished, handleFirmwareUpload);
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
