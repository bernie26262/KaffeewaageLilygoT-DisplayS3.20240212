#include "coffee_web.h"
#include <ArduinoJson.h>
#include <SPIFFS.h>

#include "coffee_wifi.h"

static AsyncWebSocket ws("/ws");
static CoffeeWebCommandHandler commandHandler = nullptr;

static const uint8_t EMPTY_PWA_ASSET[] PROGMEM = { 0x00 };

static void sendOptionalPwaAssetEmpty(AsyncWebServerRequest* request, const char* contentType)
{
  // During the T4-S3 migration the WebUI itself is served from PROGMEM.
  // The optional PWA icons may or may not be present in SPIFFS yet.
  // Do not probe SPIFFS during page load. Avoid zero-length responses here:
  // with some ESPAsyncWebServer/client combinations the favicon request kept
  // the connection open. A one-byte PROGMEM response has a known
  // Content-Length and completes reliably. Browsers simply ignore it as an
  // unusable placeholder icon.
  if (!request) {
    return;
  }
  if (!contentType || contentType[0] == '\0') {
    contentType = "application/octet-stream";
  }

  AsyncWebServerResponse* response = request->beginResponse_P(
    200,
    contentType,
    EMPTY_PWA_ASSET,
    sizeof(EMPTY_PWA_ASSET));
  response->addHeader("Cache-Control", "no-store");
  request->send(response);
}

// =============================================================================
// Eingebettete WebUI
// =============================================================================
// Aktuell bewusst als PROGMEM-String eingebettet, damit Firmware und WebUI
// immer zusammenpassen. Falls HTML/CSS/JS spaeter nach LittleFS wandern, sollte
// die fachliche Logik trotzdem weiterhin im Core/AppState bleiben.

static const char INDEX_HTML[] PROGMEM = R"rawliteral(<!doctype html>
<html lang="de">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Single-Dose-Waage</title>
  <link rel="manifest" href="/manifest.json?v=1">
  <link rel="icon" href="/favicon.ico?v=1">
  <link rel="apple-touch-icon" href="/icon-192.png?v=1">
  <meta name="theme-color" content="#061018">
  <meta name="mobile-web-app-capable" content="yes">
  <meta name="apple-mobile-web-app-capable" content="yes">
  <meta name="apple-mobile-web-app-title" content="Kaffeewaage">
  <link rel="stylesheet" href="/coffee.css?v=3b">
</head>
<body>
<main>
  <section class="card top">
    <h1>Single-Dose-Waage</h1>
    <div class="top-status">
      <span id="wifiSignalHeaderIcon" class="wifi-signal header-wifi-signal" aria-hidden="true" title="WLAN-Signal"></span>
      <span id="ws" class="pill">Getrennt</span>
    </div>
  </section>

  <section class="bottom-nav-shell" aria-label="Hauptnavigation">
    <nav class="tab-nav">
      <button class="tab-button active" type="button" data-tab="scalePage"><span class="tab-icon" aria-hidden="true">⚖</span><span>Waage</span></button>
      <button class="tab-button" type="button" data-tab="timerPage"><span class="tab-icon" aria-hidden="true">⏱</span><span>Stoppuhr</span></button>
      <button class="tab-button" type="button" data-tab="statsPage"><span class="tab-icon" aria-hidden="true">▦</span><span>Daten</span></button>
      <button class="tab-button" type="button" data-tab="settingsPage"><span class="tab-icon" aria-hidden="true">⚙</span><span>Einstellungen</span></button>
    </nav>
  </section>

  <div id="scalePage" class="page">
  <section class="card scale-card">
    <div class="weight-head">
      <div class="label">Gewicht</div>
      <div class="autodetect-row small">
        <span>Autodetect</span>
        <button id="autodetectToggle" class="autodetect-toggle off" type="button" aria-pressed="false">
          <span id="autodetectLed" class="autodetect-led"></span>
          <span id="autodetectStatus">---</span>
        </button>
      </div>
    </div>
    <div class="weight"><span id="actual">--.-</span><span class="unit">g</span></div>
    <span style="display:none">Status: <b><span id="status">---</span></b></span>

    <div class="scale-controls">
      <div class="scale-target-row small">
        <span class="label">Sollgewicht</span>
        <input id="targetWeight" type="number" inputmode="decimal" min="0.1" max="60.0" step="0.1" aria-label="Sollgewicht in Gramm">
        <span>g</span>
        <button id="targetSave" class="compact secondary" style="min-width: 110px; padding: 8px 12px; font-size: .9rem;">Speichern</button>
      </div>

      <div class="button-row">
        <button id="tare" class="compact secondary">Tara</button>
        <button id="save" class="compact" disabled>Save Dose</button>
      </div>


      <div class="scale-select-row small">
        <span class="label">Siebträger</span>
        <select id="siebtraegerSelect" aria-label="Siebträger auswählen">
          <option value="0">Bodenloser ST</option>
          <option value="1">1er-Siebträger</option>
          <option value="2">2er-Siebträger</option>
          <option value="3">Custom-ST</option>
        </select>
      </div>
    </div>
  </section>
  </div>

  <div id="timerPage" class="page hidden">
  <section class="card">
    <div class="label">Stoppuhr</div>
    <div class="value" id="stopwatch">00:00:00:0</div>
    <div class="grid" style="margin-top: 12px;">
      <button id="swToggle" class="secondary" disabled>Start / Stop</button>
      <button id="swReset" class="secondary" disabled>Reset</button>
    </div>
  </section>
  </div>

  <div id="statsPage" class="page hidden">
  <section id="maintenanceCard" class="card maintenance maintenance-alert ok">
    <div class="maintenance-title" id="maintenanceTitle">Wartung: ok</div>
    <ul class="maintenance-list" id="maintenanceList"></ul>
  </section>

  <section class="card">
    <div class="stats-grid">
      <div>
        <div class="stats-title">Shots</div>
        <div class="stat-row"><span>gesamt:</span><span id="shotsTotal">0</span></div>
        <div class="stat-row"><span>seit Reinigung Kaffeemaschine:</span><span id="shotsMachine">0</span></div>
        <div class="stat-row"><span>seit Reinigung Mühle:</span><span id="shotsGrinder">0</span></div>
        <div class="stat-row"><span>seit Filterwechsel:</span><span id="shotsFilter">0</span></div>
      </div>
      <div>
        <div class="stats-title">Mahlgut</div>
        <div class="stat-row"><span>gesamt:</span><span><span id="groundTotalKg">0.00</span> kg</span></div>
        <div class="stat-row"><span>seit Reinigung Kaffeemaschine:</span><span><span id="groundMachineG">0</span> g</span></div>
        <div class="stat-row"><span>seit Reinigung Mühle:</span><span><span id="groundGrinderG">0</span> g</span></div>
        <div class="stat-row"><span>seit Filterwechsel:</span><span><span id="groundFilterG">0</span> g</span></div>
      </div>
    </div>
    <div class="ip-row small">
      <div>Datum/Zeit: <b class="mono" id="datetime">---</b></div>
      <div>Uptime: <b class="mono" id="uptime">---</b></div>
      <div>SSID: <b class="mono" id="statsWifiSsid">---</b></div>
      <div>IP-Adresse: <b class="mono" id="ip">---</b></div>
      <div>WLAN-Signal: <span class="wifi-signal-row"><span id="statsWifiSignalIcon" class="wifi-signal" aria-hidden="true"></span> <b id="statsWifiSignalLabel">---</b> <span class="muted-inline" id="statsWifiSignalRssi">---</span></span></div>
    </div>
  </section>
  </div>

  <div id="settingsPage" class="page hidden">
  <section class="card settings-subnav-card">
    <div class="settings-subnav-title">Einstellungen</div>
    <nav class="settings-subnav" aria-label="Einstellungen">
      <button class="settings-tab-button active" type="button" data-settings-tab="settingsMaintenancePanel">Wartung</button>
      <button class="settings-tab-button" type="button" data-settings-tab="settingsScalePanel">Waage & Gefäße</button>
      <button class="settings-tab-button" type="button" data-settings-tab="settingsWifiPanel">WLAN</button>
      <button class="settings-tab-button" type="button" data-settings-tab="settingsSystemPanel">System / OTA</button>
    </nav>
  </section>

  <div id="settingsMaintenancePanel" class="settings-section">
    <div class="settings-section-title">Wartung</div>
    <div class="settings-section-hint">Häufig genutzte Wartungsanzeigen und Reset-Funktionen.</div>
    <section id="maintenanceDetailCard" class="card maintenance ok show">
      <div class="stats-title">Wartung</div>
      <div class="maintenance-title" id="maintenanceDetailTitle">Wartungszeiten</div>
      <ul class="maintenance-list" id="maintenanceDetailList"></ul>
      <div class="label" style="margin-top: 14px;">Wartung zurücksetzen</div>
      <div class="settings-actions">
        <button id="resetMachine" class="secondary">Kaffeemaschine gereinigt</button>
        <button id="resetGrinder" class="secondary">Mühle gereinigt</button>
        <button id="resetFilter" class="secondary">Filter gewechselt</button>
      </div>
    </section>
  </div>

  <div id="settingsScalePanel" class="settings-section hidden">
    <div class="settings-section-title">Waage & Gefäße</div>
    <section class="card">
      <div class="stats-title">Waage kalibrieren</div>
      <div class="small">Geführter Assistent zum Tarieren, Eingeben des Kalibriergewichts und Speichern des Kalibrierfaktors.</div>
      <div class="settings-actions" style="margin-top: 12px;">
        <button id="openCalibrate" class="secondary">Waage kalibrieren</button>
      </div>
    </section>

    <section class="card">
      <div class="stats-title">Gefäße einmessen</div>
      <div class="small">Gespeicherte Gefäßgewichte für Autodetect.</div>
      <div id="gefaessList" class="gefaess-list"></div>
      <div class="settings-actions" style="margin-top: 12px;">
        <button id="openMeasureGefaess" class="secondary">Gefäß einmessen</button>
      </div>
    </section>

    <section class="card">
      <div class="stats-title">Gesamtwerte</div>
      <div class="small">Gesamtzahl Shots und Gesamtgewicht Mahlgut manuell korrigieren.</div>
      <div class="settings-list small" style="margin-top: 12px;">
        <div>Gesamtzahl Shots: <b id="statsTotalShotsView">0</b></div>
        <div>Gesamtgewicht Mahlgut: <b id="statsTotalGroundView">0,0 g</b></div>
      </div>
      <div class="settings-actions" style="margin-top: 12px;">
        <button id="openStatsTotals" class="secondary">Gesamtwerte ändern</button>
      </div>
    </section>
  </div>

  <div id="settingsWifiPanel" class="settings-section hidden">
    <div class="settings-section-title">WLAN</div>
    <div class="settings-section-hint">Verbindung, gespeicherte WLAN-Daten und Setup-WLAN.</div>
    <section class="card">
      <div class="stats-title">WLAN-Status</div>
      <div class="settings-list small" style="margin-top: 12px;">
        <div>WLAN: <b id="wifiStatus">---</b></div>
        <div>SSID: <b class="mono" id="wifiSsid">---</b></div>
        <div>Signal: <span class="wifi-signal-row"><span id="wifiSignalIcon" class="wifi-signal" aria-hidden="true"></span> <b id="wifiSignalLabel">---</b> <span class="muted-inline" id="wifiSignalRssi">---</span></span></div>
        <div>Quelle: <b id="wifiCredentialSource">---</b></div>
        <div>Gespeicherte WLAN-Daten: <b id="wifiStoredCredentials">---</b></div>
        <div>Gespeicherte WLAN-Daten aktiv: <b id="wifiStoredCredentialsActive">---</b></div>
      </div>
      <div class="settings-list small" style="margin-top: 10px;">
        <div>Gespeicherte SSID: <b class="mono" id="wifiStoredSsid">---</b></div>
        <div>Gespeichertes Passwort: <b id="wifiStoredPasswordStatus">---</b></div>
        <div>Setup-WLAN: <b id="wifiSetupApStatus">aus</b></div>
        <div>Setup-Adresse: <b class="mono" id="wifiSetupApAddress">---</b></div>
      </div>
      <div class="small" style="margin-top: 10px;">Gespeicherte WLAN-Daten werden nur nach ausdrücklicher Aktivierung beim Neustart verwendet. Falls die Verbindung damit fehlschlägt, nutzt die Waage automatisch wieder das Standard-WLAN aus der Firmware.</div>
      <form class="wifi-form" id="wifiCredentialsForm" aria-label="WLAN-Zugangsdaten" autocomplete="off">
        <div class="stats-title" style="margin-bottom: 0;">WLAN-Zugangsdaten vorbereiten</div>
        <div class="wifi-form-note">Speichert SSID und Passwort in der Waage. Die Daten werden erst verwendet, wenn sie danach bewusst aktiviert werden.</div>
        <label><span>SSID</span><input id="wifiSetupSsid" name="ssid" type="text" autocomplete="off" placeholder="WLAN-Name"></label>
        <label><span>Passwort</span><input id="wifiSetupPassword" name="password" type="password" autocomplete="new-password" placeholder="Leer lassen, um gespeichertes Passwort zu behalten"></label>
        <div class="settings-actions">
          <button id="saveWifiCredentials" class="secondary" type="submit">WLAN-Daten speichern</button>
        </div>
      </form>
      <div class="settings-actions" style="margin-top: 12px;">
        <button id="activateWifiCredentials" class="secondary">Gespeicherte WLAN-Daten verwenden</button>
        <button id="deactivateWifiCredentials" class="secondary">Standard-WLAN verwenden</button>
        <button id="clearWifiCredentials" class="secondary">Gespeicherte WLAN-Daten löschen</button>
        <button id="startWifiSetupAp" class="secondary">Setup-WLAN starten</button>
        <button id="stopWifiSetupAp" class="secondary">Setup-WLAN stoppen</button>
      </div>
    </section>
  </div>

  <div id="settingsSystemPanel" class="settings-section hidden">
    <div class="settings-section-title">System / OTA</div>
    <section class="card">
      <div class="stats-title">System</div>
      <div class="small">Firmware-Update und Neustart.</div>
      <div class="settings-actions" style="margin-top: 12px;">
        <button id="openUpdatePage" class="secondary">Update-Seite öffnen</button>
        <button id="restartDevice" class="danger">ESP32 neu starten</button>
      </div>
    </section>

    <section class="card">
      <div class="stats-title">Log</div>
      <div class="small">Letzte WebUI-/Systemmeldung.</div>
      <div class="log mono" id="log"></div>
    </section>
  </div>

  </div>

  <div id="confirmOverlay" class="modal-backdrop" role="dialog" aria-modal="true" aria-labelledby="confirmTitle">
    <div class="modal">
      <div class="modal-title" id="confirmTitle">Wartung reset?</div>
      <div class="modal-text" id="confirmText">Bitte bestätigen.</div>
      <div class="modal-actions">
        <button id="confirmCancel" class="secondary">Abbrechen</button>
        <button id="confirmOk" class="danger">Ok</button>
      </div>
    </div>
  </div>

  <div id="wizardOverlay" class="modal-backdrop" role="dialog" aria-modal="true" aria-labelledby="wizardTitle">
    <div class="modal">
      <div class="modal-title" id="wizardTitle">Assistent</div>
      <div class="modal-text" id="wizardText">Bitte folgen.</div>
      <div id="wizardBody"></div>
      <div class="modal-actions" id="wizardActions"></div>
    </div>
  </div>

</main>

<script src="/coffee_core.js?v=3c"></script>
<script src="/coffee_render.js?v=3c"></script>
<script src="/coffee_events.js?v=3c"></script>
</body>
</html>)rawliteral";

static const char COFFEE_CSS[] PROGMEM = R"rawliteral(    /* ===== Basis / Layout ===== */
    :root {
      font-family: system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
      color: #f4f7fb;
      background: #061018;
      --bg: #061018;
      --bg-soft: #0b1722;
      --card: #101b27;
      --card-2: #142235;
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
    body {
      margin: 0;
      min-height: 100vh;
      padding: 18px 18px calc(96px + env(safe-area-inset-bottom));
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
    .top-status { display: inline-flex; align-items: center; gap: 10px; margin-left: auto; }
    h1 { margin: 0; font-size: 1.15rem; letter-spacing: -.02em; white-space: nowrap; }
    .pill {
      flex: 0 0 auto;
      padding: 6px 10px;
      border-radius: 999px;
      background: #1f2937;
      color: var(--muted-2);
      border: 1px solid var(--border);
      font-size: .82rem;
      white-space: nowrap;
    }
    .pill.ok { background: var(--accent-soft); color: #bbf7d0; border-color: rgba(85,195,66,.36); }
    /* ===== Wartung ===== */
    .maintenance { display: none; border-left: 6px solid var(--accent); }
    .maintenance-alert { order: -1; }
    .maintenance.show { display: block; }
    .maintenance.ok { border-left-color: var(--accent); background: linear-gradient(180deg, rgba(20,34,53,.96), rgba(11,31,22,.92)); }
    .maintenance.warn { border-left-color: var(--danger); background: linear-gradient(180deg, rgba(45,23,28,.96), rgba(24,16,24,.92)); }
    .maintenance-title { font-size: 1.05rem; font-weight: 750; margin-bottom: 6px; }
    .maintenance.ok .maintenance-title { color: #bbf7d0; }
    .maintenance.warn .maintenance-title { color: #fecaca; }
    .maintenance-list { margin: 0; padding-left: 18px; }
    .maintenance-item { margin: 4px 0; }
    .maintenance-item.ok { color: #bbf7d0; }
    .maintenance-item.due { color: #fecaca; font-weight: 750; }
    /* ===== Autodetect / Gewichtskarte ===== */
    .scale-card { display: grid; gap: 12px; }
    .weight-head { display: flex; justify-content: space-between; align-items: center; gap: 12px; }
    .autodetect-row { display: inline-flex; justify-content: flex-end; align-items: center; gap: 10px; margin: 0; padding: 0; border-bottom: 0; white-space: nowrap; }
    .autodetect-toggle {
      width: auto;
      min-width: 0;
      display: inline-flex;
      align-items: center;
      gap: 8px;
      padding: 8px 13px;
      border-radius: 999px;
      font-size: .9rem;
      background: #1f2937;
      color: var(--muted-2);
      border: 1px solid var(--border);
      box-shadow: none;
    }
    .autodetect-toggle.on { background: var(--accent-soft); color: #bbf7d0; border-color: rgba(85,195,66,.42); }
    .autodetect-toggle.off { background: #1f2937; color: var(--muted); }
    .autodetect-toggle.paused { background: rgba(245,158,11,.16); color: #fde68a; border-color: rgba(245,158,11,.35); }
    .autodetect-led { width: 11px; height: 11px; border-radius: 50%; background: #64748b; box-shadow: inset 0 0 0 1px rgba(0,0,0,.25); }
    .autodetect-led.on { background: var(--accent); box-shadow: 0 0 0 3px rgba(85,195,66,.20), 0 0 12px rgba(85,195,66,.75); }
    .weight {
      font-size: 4rem;
      line-height: 1;
      font-weight: 800;
      letter-spacing: -0.06em;
      margin: 12px 0 4px;
      text-align: right;
      font-variant-numeric: tabular-nums;
      color: #f8fafc;
      text-shadow: 0 10px 28px rgba(0,0,0,.36);
    }
    .weight .unit { font-size: 2rem; letter-spacing: 0; margin-left: 6px; color: var(--muted-2); }
    .scale-controls { display: grid; gap: 12px; margin-top: 14px; }
    .scale-target-row, .scale-select-row { display: flex; align-items: center; gap: 8px; flex-wrap: wrap; }
    .scale-target-row .label, .scale-select-row .label { min-width: 92px; }
    .weight-footer { display: flex; justify-content: space-between; align-items: end; gap: 12px; flex-wrap: wrap; }
    .weight-info { min-width: 0; }
    .grid { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 12px; }
    .label { color: var(--muted); font-size: .86rem; }
    .value { font-size: 1.25rem; font-weight: 650; margin-top: 4px; color: var(--text); }
    button {
      width: 100%;
      border: 1px solid rgba(85,195,66,.36);
      border-radius: 16px;
      padding: 16px;
      font-size: 1.15rem;
      font-weight: 750;
      background: linear-gradient(180deg, var(--accent), var(--accent-2));
      color: #f8fff8;
      cursor: pointer;
      box-shadow: 0 12px 30px rgba(63,167,45,.20);
    }
    button.compact { width: auto; min-width: 150px; padding: 13px 18px; }
    button.secondary {
      background: #1a2533;
      color: var(--text);
      border-color: rgba(85,195,66,.46);
      box-shadow: none;
    }
    .button-row { display: flex; gap: 10px; align-items: center; flex-wrap: wrap; }
    button:disabled {
      background: #334155;
      color: #94a3b8;
      border-color: rgba(148,163,184,.20);
      cursor: not-allowed;
      box-shadow: none;
    }
    select, input {
      border: 1px solid var(--border-strong);
      border-radius: 12px;
      padding: 7px 10px;
      font: inherit;
      font-size: .9rem;
      background: #0b1520;
      color: var(--text);
    }
    select:focus, input:focus, button:focus-visible {
      outline: 2px solid rgba(85,195,66,.55);
      outline-offset: 2px;
    }
    input[type=number] { width: 86px; font-variant-numeric: tabular-nums; }
    .small { font-size: .9rem; color: var(--muted); }
    .mono { font-family: ui-monospace, SFMono-Regular, Consolas, monospace; }
    /* ===== Status / Statistik / Settings ===== */
    .log {
      margin-top: 12px;
      padding: 10px 12px;
      border-radius: 14px;
      background: #0b1520;
      border: 1px solid var(--border);
      font-size: .9rem;
      color: var(--muted-2);
      min-height: 1.2em;
    }
    .log:empty { display: none; }
    .stats-grid { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 18px; }
    .stats-title { font-size: 1.05rem; font-weight: 750; margin-bottom: 8px; color: var(--text); }
    .stat-row { display: flex; justify-content: space-between; gap: 12px; padding: 6px 0; border-bottom: 1px solid rgba(148,163,184,.14); }
    .stat-row:last-child { border-bottom: 0; }
    .stat-row span:first-child { color: var(--muted); }
    .stat-row span:last-child { font-weight: 650; text-align: right; font-variant-numeric: tabular-nums; color: var(--text); }
    .ip-row { margin-top: 14px; padding-top: 12px; border-top: 1px solid rgba(148,163,184,.18); }
    .settings-list { display: grid; gap: 8px; margin-top: 8px; }
    .wifi-form { display: grid; gap: 10px; margin-top: 14px; padding-top: 14px; border-top: 1px solid rgba(148,163,184,.18); }
    .wifi-form label { display: grid; gap: 5px; color: var(--muted); font-size: .9rem; }
    .wifi-form input { width: 100%; box-sizing: border-box; }
    .wifi-form-note { color: var(--muted); font-size: .86rem; line-height: 1.35; }
    .wifi-signal-row { display: inline-flex; align-items: center; gap: 6px; flex-wrap: wrap; }
    .wifi-signal { display: inline-flex; align-items: flex-end; gap: 2px; height: 14px; vertical-align: -2px; }
    .wifi-signal .bar { display: block; width: 4px; border-radius: 2px 2px 0 0; background: rgba(148,163,184,.35); }
    .wifi-signal .bar:nth-child(1) { height: 5px; }
    .wifi-signal .bar:nth-child(2) { height: 8px; }
    .wifi-signal .bar:nth-child(3) { height: 11px; }
    .wifi-signal .bar:nth-child(4) { height: 14px; }
    .wifi-signal .bar.on { background: #22c55e; box-shadow: 0 0 8px rgba(34,197,94,.35); }
    .header-wifi-signal { height: 18px; padding: 4px 2px; }
    .header-wifi-signal .bar { width: 5px; }
    .header-wifi-signal .bar:nth-child(1) { height: 6px; }
    .header-wifi-signal .bar:nth-child(2) { height: 10px; }
    .header-wifi-signal .bar:nth-child(3) { height: 14px; }
    .header-wifi-signal .bar:nth-child(4) { height: 18px; }
    .muted-inline { color: var(--muted); }
    .dashboard-settings-list { gap: 4px; margin-top: 6px; line-height: 1.25; }
    .settings-actions { display: grid; gap: 10px; margin-top: 12px; }
    .settings-section { display: grid; gap: 10px; }
    .settings-section.hidden { display: none; }
    .settings-subnav-card { padding: 12px; }
    .settings-subnav-title { margin: 0 0 10px; color: var(--muted); font-size: .82rem; font-weight: 800; letter-spacing: .06em; text-transform: uppercase; }
    .settings-subnav {
      display: grid;
      grid-template-columns: repeat(4, minmax(0, 1fr));
      gap: 6px;
      padding: 6px;
      border: 1px solid var(--border);
      border-radius: 20px;
      background: rgba(6,16,24,.48);
    }
    .settings-tab-button {
      width: 100%;
      min-width: 0;
      min-height: 46px;
      padding: 9px 8px;
      border-radius: 15px;
      font-size: .84rem;
      line-height: 1.15;
      background: transparent;
      color: var(--muted-2);
      border-color: transparent;
      box-shadow: none;
    }
    .settings-tab-button.active { background: var(--accent-soft); color: #bbf7d0; border-color: rgba(85,195,66,.42); box-shadow: none; }
    .settings-tab-button:active { transform: translateY(1px); }
    .settings-section-title { margin: 4px 2px 0; color: var(--muted); font-size: .82rem; font-weight: 800; letter-spacing: .06em; text-transform: uppercase; }
    .settings-section-hint { margin: -2px 2px 2px; color: var(--muted); font-size: .86rem; }
    .gefaess-list { display: grid; gap: 8px; margin-top: 10px; }
    .gefaess-row { display: flex; justify-content: space-between; align-items: center; gap: 12px; padding: 9px 0; border-bottom: 1px solid rgba(148,163,184,.14); }
    .gefaess-row:last-child { border-bottom: 0; }
    .gefaess-weight { font-weight: 700; }
    .gefaess-missing { color: var(--muted); }
    .page { display: grid; gap: 14px; }
    .page.hidden { display: none; }
    .bottom-nav-shell {
      position: fixed;
      left: 0;
      right: 0;
      bottom: 0;
      z-index: 900;
      padding: 10px 14px calc(10px + env(safe-area-inset-bottom));
      background: linear-gradient(180deg, rgba(6,16,24,0), rgba(4,10,16,.98) 30%, rgba(4,10,16,.98));
      backdrop-filter: blur(14px);
    }
    .tab-nav {
      max-width: 760px;
      margin: 0 auto;
      display: grid;
      grid-template-columns: repeat(4, minmax(0, 1fr));
      gap: 8px;
      padding: 8px;
      border: 1px solid var(--border);
      border-radius: 24px;
      background: rgba(16,27,39,.94);
      box-shadow: 0 18px 48px rgba(0,0,0,.42);
    }
    .tab-button {
      width: 100%;
      min-width: 0;
      min-height: 58px;
      display: grid;
      place-items: center;
      gap: 3px;
      padding: 7px 5px;
      border-radius: 18px;
      font-size: .78rem;
      line-height: 1.1;
      background: transparent;
      color: var(--muted-2);
      border-color: transparent;
      box-shadow: none;
    }
    .tab-button .tab-icon { font-size: 1.22rem; line-height: 1; }
    .tab-button.active {
      background: linear-gradient(180deg, var(--accent), var(--accent-2));
      color: #f8fff8;
      border-color: rgba(85,195,66,.48);
      box-shadow: 0 10px 24px rgba(63,167,45,.28);
    }
    .nav-button { width: auto; min-width: 150px; padding: 10px 14px; font-size: .95rem; }
    /* ===== Overlay / Wizard ===== */
    .modal-backdrop { position: fixed; inset: 0; display: none; place-items: center; padding: 18px; background: rgba(2, 6, 12, .72); z-index: 1000; }
    .modal-backdrop.show { display: grid; }
    .modal {
      width: min(420px, 100%);
      background: linear-gradient(180deg, var(--card-2), var(--card));
      border: 1px solid var(--border);
      border-radius: 22px;
      padding: 20px;
      box-shadow: 0 24px 70px rgba(0,0,0,.48);
    }
    .modal-title { font-size: 1.25rem; font-weight: 800; margin-bottom: 8px; }
    .modal-text { color: var(--muted-2); margin-bottom: 16px; white-space: pre-line; }
    .modal-actions { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }
    .wizard-field { display: grid; gap: 6px; margin: 12px 0; }
    .wizard-field input, .wizard-field select { width: 100%; box-sizing: border-box; padding: 11px 12px; font-size: 1rem; }
    .wizard-note { background: #0b1520; border: 1px solid var(--border); border-radius: 14px; padding: 10px 12px; margin: 12px 0; font-size: .92rem; color: var(--muted-2); }
    .wizard-note.warn { background: rgba(245,158,11,.14); border-color: rgba(245,158,11,.32); color: #fde68a; }
    .danger { background: linear-gradient(180deg, #ef4444, #b91c1c); border-color: rgba(248,113,113,.45); }
    @media (max-width: 640px) {
      body { padding: 14px 14px calc(96px + env(safe-area-inset-bottom)); }
      .grid, .stats-grid { grid-template-columns: 1fr; }
      .weight { font-size: 3.4rem; }
      .weight-head { align-items: flex-start; }
      .autodetect-row { margin-left: auto; }
      button.compact, .button-row, .nav-button { width: 100%; }
      .modal-actions { grid-template-columns: 1fr; }
    }
)rawliteral";

static const char COFFEE_CORE_JS[] PROGMEM = R"rawliteral(// ===== WebSocket / globaler UI-State =====
let ws;
let lastState = null;
let maintenanceDueAtMs = { machine: 0, grinder: 0, filter: 0 };
let stopwatchBaseClientMs = 0;
let stopwatchBaseMs = 0;
let stopwatchRunning = false;
let targetWeightDirty = false;
let pendingConfirm = null;
let wizard = { type: null, step: 0, calibrationWeight: null };
let wizardEndSent = true;
const el = id => document.getElementById(id);
const setText = (id, value) => { el(id).textContent = value; };
const GEFAESS_COUNT = 3;
const GEFAESS_INDEXES = Array.from({ length: GEFAESS_COUNT }, (_, index) => index);
const GEFAESS_NAMES = ['Gefäß 1', 'Gefäß 2', 'Gefäß 3'];
const CMD = Object.freeze({
  saveDose: 'save_dose',
  tare: 'tare',
  stopwatchStartStop: 'stopwatch_start_stop',
  stopwatchReset: 'stopwatch_reset',
  autodetectOn: 'autodetect_on',
  autodetectOff: 'autodetect_off',
  wizardEnd: 'web_wizard_end',
  wizardTare: 'web_wizard_tare',
  scaleCalibrationApply: 'scale_calibration_apply',
  measureGefaessSave: 'measure_gefaess_save',
  maintenanceResetMachine: 'maintenance_reset_machine',
  maintenanceResetGrinder: 'maintenance_reset_grinder',
  maintenanceResetFilter: 'maintenance_reset_filter',
  restartDevice: 'restart_device',
  setStatsTotalsPrefix: 'set_stats_totals_'
});
const fmtG = v => {
  let n = Number(v || 0);
  if (n > -0.05 && n < 0.05) n = 0;
  return n.toFixed(1);
};
const fmtKg = g => (Number(g || 0) / 1000).toFixed(2);
const fmtWholeG = g => String(Math.round(Number(g || 0)));
const pad2 = n => String(Math.max(0, Math.floor(Number(n) || 0))).padStart(2, '0');
const fmtStopwatchTime = ms => {
  ms = Number(ms || 0);
  const totalSeconds = Math.floor(ms / 1000);
  const h = Math.floor(totalSeconds / 3600);
  const m = Math.floor((totalSeconds % 3600) / 60);
  const s = totalSeconds % 60;
  const d = Math.floor((ms % 1000) / 100);
  return `${pad2(h)}:${pad2(m)}:${pad2(s)}:${d}`;
};
const fmtDayClockDuration = seconds => {
  const total = Math.max(0, Math.floor(Math.abs(Number(seconds) || 0)));
  const days = Math.floor(total / 86400);
  const h = Math.floor((total % 86400) / 3600);
  const m = Math.floor((total % 3600) / 60);
  const s = total % 60;
  const clock = `${pad2(h)}:${pad2(m)}:${pad2(s)}`;
  if (days === 1) return `1 Tag, ${clock}`;
  if (days > 1) return `${days} Tage, ${clock}`;
  return clock;
};
const fmtDateTime = (epoch, valid) => {
  if (!valid || !epoch) return 'nicht synchronisiert';
  return new Date(Number(epoch) * 1000).toLocaleString('de-DE', {
    day: '2-digit', month: '2-digit', year: 'numeric',
    hour: '2-digit', minute: '2-digit', second: '2-digit'
  });
};
const fmtUptime = ms => fmtDayClockDuration(Math.floor(Number(ms || 0) / 1000));

// ===== Navigation / Log =====
function addLog(msg) {
  const now = new Date().toLocaleTimeString();
  el('log').textContent = `${now}  ${msg}`;
}

function showTab(targetPageId) {
  ['scalePage', 'timerPage', 'statsPage', 'settingsPage'].forEach(pageId => {
    el(pageId).classList.toggle('hidden', pageId !== targetPageId);
  });
  document.querySelectorAll('.tab-button').forEach(button => {
    const active = button.dataset.tab === targetPageId;
    button.classList.toggle('active', active);
    button.setAttribute('aria-current', active ? 'page' : 'false');
  });

  // Beim Seitenwechsel immer oben starten.
  // Sonst bleibt die Scrollposition der vorherigen Seite erhalten.
  window.scrollTo({ top: 0, left: 0, behavior: 'auto' });
}

function showSettingsTab(targetPanelId) {
  ['settingsMaintenancePanel', 'settingsScalePanel', 'settingsWifiPanel', 'settingsSystemPanel'].forEach(panelId => {
    el(panelId).classList.toggle('hidden', panelId !== targetPanelId);
  });
  document.querySelectorAll('.settings-tab-button').forEach(button => {
    const active = button.dataset.settingsTab === targetPanelId;
    button.classList.toggle('active', active);
    button.setAttribute('aria-current', active ? 'page' : 'false');
  });
}

// ===== Bestaetigungs-Overlay =====
function resetConfirmOverlayButtons() {
  el('confirmCancel').style.display = '';
  el('confirmOk').classList.add('danger');
  el('confirmOk').classList.remove('secondary');
  el('confirmOk').textContent = 'Ok';
}

function openConfirmOverlay(title, text, cmd, logText) {
  pendingConfirm = { cmd, logText };
  resetConfirmOverlayButtons();
  el('confirmTitle').textContent = title;
  el('confirmText').textContent = text;
  el('confirmOverlay').classList.add('show');
  el('confirmCancel').focus();
}

function openInfoOverlay(title, text) {
  pendingConfirm = null;
  el('confirmCancel').style.display = 'none';
  el('confirmOk').classList.remove('danger');
  el('confirmOk').classList.add('secondary');
  el('confirmOk').textContent = 'OK';
  el('confirmTitle').textContent = title;
  el('confirmText').textContent = text;
  el('confirmOverlay').classList.add('show');
  el('confirmOk').focus();
}

function closeConfirmOverlay() {
  el('confirmOverlay').classList.remove('show');
  pendingConfirm = null;
  resetConfirmOverlayButtons();
}

function confirmPendingAction() {
  if (!pendingConfirm) {
    closeConfirmOverlay();
    return;
  }
  const action = pendingConfirm;
  closeConfirmOverlay();
  if (action.cmd === 'clear_wifi_credentials_webui') {
    clearWifiCredentials();
    return;
  }
  if (action.cmd === 'activate_wifi_credentials_webui') {
    setWifiCredentialsActive(true);
    return;
  }
  if (action.cmd === 'deactivate_wifi_credentials_webui') {
    setWifiCredentialsActive(false);
    return;
  }
  sendCommand(action.cmd, action.logText);
}

// ===== WebSocket-Kommandos =====
function isWebSocketReady() {
  return !!ws && ws.readyState === WebSocket.OPEN;
}

function sendCommand(cmd, logText) {
  if (!isWebSocketReady()) {
    addLog('WebSocket nicht verbunden');
    return;
  }
  ws.send(JSON.stringify({ cmd }));
  addLog(logText || `${cmd} gesendet …`);
}

function sendCommandAndThen(cmd, logText, nextStep) {
  sendCommand(cmd, logText);
  if (typeof nextStep === 'number') {
    wizard.step = nextStep;
    renderWizard();
  }
}

// ===== Wizard-Overlay =====
function finishWizardOverlay() {
  if (!wizardEndSent) {
    sendCommand(CMD.wizardEnd, 'Assistent beendet, Autodetect wiederherstellen …');
    wizardEndSent = true;
  }
}

function closeWizardOverlay() {
  finishWizardOverlay();
  el('wizardOverlay').classList.remove('show');
  wizard = { type: null, step: 0 };
}

function wizardButton(label, className, onClick) {
  const btn = document.createElement('button');
  btn.textContent = label;
  if (className) btn.className = className;
  btn.addEventListener('click', onClick);
  return btn;
}

function wizardPausedNote() {
  return lastState?.system?.autodetect_paused
    ? '<div class="wizard-note warn">Autodetect ist während dieses Assistenten pausiert, damit keine Messung automatisch tariert wird.</div>'
    : '';
}

function setWizardContent(title, text, bodyHtml, actions) {
  el('wizardTitle').textContent = title;
  el('wizardText').textContent = text;
  el('wizardBody').innerHTML = (bodyHtml || '') + wizardPausedNote();
  const actionBox = el('wizardActions');
  actionBox.innerHTML = '';
  actions.forEach(action => actionBox.appendChild(action));
}

function renderCalibrationWizard() {
  if (wizard.step === 0) {
    setWizardContent(
      'Waage kalibrieren: Tarieren',
      'Bitte die Waage vollständig leeren und dann tarieren.',
      `<div class="wizard-note">Aktuelles Gewicht: <b class="wizardLiveWeight">${fmtG(lastState?.weight?.actual_g)} g</b><br>Die Anzeige sollte nach dem Tarieren nahe 0,0 g stehen.</div>`,
      [
        wizardButton('Abbrechen', 'secondary', closeWizardOverlay),
        wizardButton('Tarieren', '', () => sendCommandAndThen(CMD.wizardTare, 'Kalibrierung: Tara gesendet …', 1))
      ]
    );
    return;
  }

  if (wizard.step === 1) {
    const current = Number(lastState?.calibration?.set_weight_g || 200).toFixed(1);
    setWizardContent(
      'Waage kalibrieren: Gewicht einstellen',
      'Bitte das Kalibriergewicht auflegen und den bekannten Gewichtswert eintragen.',
      `<label class="wizard-field"><span>Kalibriergewicht in g</span><input id="calibrationWeightInput" type="text" inputmode="decimal" value="${current}"></label>`,
      [
        wizardButton('Zurück', 'secondary', () => { wizard.step = 0; renderWizard(); }),
        wizardButton('Weiter', '', () => {
          const value = Number(el('calibrationWeightInput').value.replace(',', '.'));
          if (!Number.isFinite(value) || value <= 0) { addLog('Ungültiges Kalibriergewicht'); return; }
          wizard.calibrationWeight = value;
          sendCommandAndThen(`scale_calibration_set_weight_${value.toFixed(1)}`, `Kalibriergewicht gesendet: ${value.toFixed(1)} g`, 2);
        })
      ]
    );
    el('calibrationWeightInput').focus();
    return;
  }

  if (wizard.step === 2) {
    const current = Number(wizard.calibrationWeight ?? lastState?.calibration?.set_weight_g ?? 0).toFixed(1);
    setWizardContent(
      'Waage kalibrieren: Kalibrieren',
      `Bitte warten, bis das Gewicht ruhig steht. Eingestelltes Kalibriergewicht: ${current} g.`,
      '<div class="wizard-note">Danach wird der Kalibrierfaktor berechnet und gespeichert.</div>',
      [
        wizardButton('Zurück', 'secondary', () => { wizard.step = 1; renderWizard(); }),
        wizardButton('Kalibrieren', '', () => {
          const value = Number(wizard.calibrationWeight ?? lastState?.calibration?.set_weight_g ?? 0);
          if (!Number.isFinite(value) || value <= 0) { addLog('Ungültiges Kalibriergewicht'); return; }
          sendCommandAndThen(`scale_calibration_apply_${value.toFixed(1)}`, 'Kalibrierung gesendet …', 3);
        })
      ]
    );
    return;
  }

  const factor = Number(lastState?.calibration?.factor || 0).toFixed(4);
  const known = Number(lastState?.calibration?.set_weight_g || 0).toFixed(1);
  setWizardContent(
    'Waage kalibrieren: OK',
    'Die Kalibrierung wurde gespeichert.',
    `<div class="wizard-note">Kalibriergewicht: ${known} g<br>Kalibrierfaktor: ${factor}</div>`,
    [wizardButton('Fertig', '', closeWizardOverlay)]
  );
}

function gefaessName(index) {
  return GEFAESS_NAMES[index] || `Gefäß ${index + 1}`;
}

function renderMeasureGefaessWizard() {
  if (wizard.step === 0) {
    const current = Number(lastState?.selection?.gefaess ?? 0);
    const options = GEFAESS_INDEXES.map(i => `<option value="${i}" ${i === current ? 'selected' : ''}>${gefaessName(i)}</option>`).join('');
    setWizardContent(
      'Gefäße einmessen: Auswahl',
      'Bitte auswählen, welches Gefäß eingemessen werden soll.',
      `<label class="wizard-field"><span>Gefäß</span><select id="gefaessWizardSelect">${options}</select></label>`,
      [
        wizardButton('Abbrechen', 'secondary', closeWizardOverlay),
        wizardButton('Weiter', '', () => {
          const idx = Number(el('gefaessWizardSelect').value);
          sendCommandAndThen(`select_gefaess_${idx}`, `${gefaessName(idx)} ausgewählt …`, 1);
        })
      ]
    );
    return;
  }

  if (wizard.step === 1) {
    setWizardContent(
      'Gefäße einmessen: Tarieren',
      'Bitte die Waage vollständig leeren und dann tarieren.',
      `<div class="wizard-note">Aktuelles Gewicht: <b class="wizardLiveWeight">${fmtG(lastState?.weight?.actual_g)} g</b><br>Danach das ausgewählte Gefäß auflegen.</div>`,
      [
        wizardButton('Zurück', 'secondary', () => { wizard.step = 0; renderWizard(); }),
        wizardButton('Tarieren', '', () => sendCommandAndThen(CMD.wizardTare, 'Gefäß einmessen: Tara gesendet …', 2))
      ]
    );
    return;
  }

  if (wizard.step === 2) {
    const idx = Number(lastState?.selection?.gefaess ?? 0);
    setWizardContent(
      'Gefäße einmessen: Gefäß auflegen',
      `Bitte ${gefaessName(idx)} auflegen und warten, bis das Gewicht ruhig steht.`,
      `<div class="wizard-note">Aktuelles Gewicht: <b id="gefaessLiveWeight">${fmtG(lastState?.weight?.actual_g)} g</b></div>`,
      [
        wizardButton('Zurück', 'secondary', () => { wizard.step = 1; renderWizard(); }),
        wizardButton('Gewicht speichern', '', () => sendCommandAndThen(CMD.measureGefaessSave, 'Gefäßgewicht speichern gesendet …', 3))
      ]
    );
    return;
  }

  const idx = Number(lastState?.selection?.gefaess ?? 0);
  const weights = lastState?.gefaesse?.weights_g || [];
  const weight = Number(weights[idx] || lastState?.weight?.actual_g || 0).toFixed(1);
  setWizardContent(
    'Gefäße einmessen: OK',
    `${gefaessName(idx)} wurde gespeichert.`,
    `<div class="wizard-note">Gespeichertes Gewicht: ${weight} g</div>`,
    [
      wizardButton('Weiteres Gefäß', 'secondary', () => { wizard.step = 0; renderWizard(); }),
      wizardButton('Fertig', '', closeWizardOverlay)
    ]
  );
}

function renderStatsTotalsWizard() {
  const currentShots = Number(lastState?.stats?.shots?.total ?? 0);
  const currentGround = fmtG(lastState?.stats?.ground?.total_g);
  setWizardContent(
    'Gesamtwerte wirklich verändern?',
    'Hier können die Gesamtzahl der Shots und das gesamte Mahlgut korrigiert werden. Wartungszähler seit Reinigung oder Filterwechsel bleiben unverändert.',
    `<label class="wizard-field"><span>Gesamtzahl Shots</span><input id="editShotsTotal" type="number" min="0" step="1" inputmode="numeric" value="${currentShots}"></label>` +
    `<label class="wizard-field"><span>Gesamtgewicht Mahlgut in g</span><input id="editGroundTotal" type="text" inputmode="decimal" value="${currentGround}"></label>`,
    [
      wizardButton('Abbrechen', 'secondary', closeWizardOverlay),
      wizardButton('Speichern', '', () => {
        const shots = Number(el('editShotsTotal').value);
        const ground = Number(el('editGroundTotal').value.replace(',', '.'));

        if (!Number.isInteger(shots) || shots < 0) {
          addLog('Ungültige Gesamtzahl Shots');
          return;
        }
        if (!Number.isFinite(ground) || ground < 0) {
          addLog('Ungültiges Gesamtgewicht Mahlgut');
          return;
        }

        const groundTenths = Math.round(ground * 10);
        const groundText = (groundTenths / 10).toFixed(1).replace('.', ',');
        sendCommand(`${CMD.setStatsTotalsPrefix}${shots}_${groundTenths}`, `Gesamtwerte ändern gesendet: ${shots} Shots, ${groundText} g`);
        closeWizardOverlay();
      })
    ]
  );
  el('editShotsTotal').focus();
}

function renderWizard() {
  if (wizard.type === 'calibration') renderCalibrationWizard();
  if (wizard.type === 'gefaess') renderMeasureGefaessWizard();
  if (wizard.type === 'statsTotals') renderStatsTotalsWizard();
}

function updateWizardLiveFields() {
  const currentWeight = `${fmtG(lastState?.weight?.actual_g)} g`;
  document.querySelectorAll('.wizardLiveWeight').forEach(node => {
    node.textContent = currentWeight;
  });
  const weightEl = el('gefaessLiveWeight');
  if (weightEl) {
    weightEl.textContent = currentWeight;
  }
}

function openWizard(type, pauseAutodetect = true) {
  wizard = { type, step: 0 };
  wizardEndSent = !pauseAutodetect;
  if (pauseAutodetect) {
    sendCommand('web_wizard_begin', 'Assistent gestartet, Autodetect pausieren …');
  }
  el('wizardOverlay').classList.add('show');
  renderWizard();
}

function openCalibrationWizard() {
  openWizard('calibration');
}

function openMeasureGefaessWizard() {
  openWizard('gefaess');
}

function openStatsTotalsWizard() {
  openWizard('statsTotals', false);
}
)rawliteral";

static const char COFFEE_RENDER_JS[] PROGMEM = R"rawliteral(// ===== Wartung =====
function fmtDuration(seconds) {
  return fmtDayClockDuration(seconds);
}

function maintenanceLine(label, secondsToDue) {
  const seconds = Number(secondsToDue) || 0;
  const due = seconds < 0;
  const text = due
    ? `${label}: seit ${fmtDuration(seconds)} fällig`
    : `${label}: fällig in ${fmtDuration(seconds)}`;
  return { text, due };
}

function syncMaintenanceDueAt(key, secondsToDue) {
  const seconds = Number(secondsToDue) || 0;
  const candidateDueAtMs = Date.now() + seconds * 1000;
  const currentDueAtMs = maintenanceDueAtMs[key] || 0;

  // Neue ESP-States kommen nicht exakt im Sekundentakt. Kleine Abweichungen
  // ignorieren wir, damit die Anzeige nicht springt. Echte Änderungen durch
  // Reset-/Debug-Commands übernehmen wir sofort.
  if (!currentDueAtMs || Math.abs(candidateDueAtMs - currentDueAtMs) > 5000) {
    maintenanceDueAtMs[key] = candidateDueAtMs;
  }
}

function syncMaintenanceTimers(m) {
  if (!lastState?.time?.valid) {
    maintenanceDueAtMs = { machine: 0, grinder: 0, filter: 0 };
    return;
  }

  syncMaintenanceDueAt('machine', m?.machine_seconds_to_due);
  syncMaintenanceDueAt('grinder', m?.grinder_seconds_to_due);
  syncMaintenanceDueAt('filter', m?.filter_seconds_to_due);
}

function adjustedMaintenanceSeconds(key, fallbackSecondsToDue) {
  const dueAtMs = maintenanceDueAtMs[key] || 0;
  if (!lastState?.time?.valid || !dueAtMs) {
    return Number(fallbackSecondsToDue) || 0;
  }

  const diffMs = dueAtMs - Date.now();
  if (diffMs >= 0) return Math.ceil(diffMs / 1000);
  return Math.floor(diffMs / 1000);
}

function renderMaintenance(m) {
  const card = el('maintenanceCard');
  const title = el('maintenanceTitle');
  const list = el('maintenanceList');
  const detailCard = el('maintenanceDetailCard');
  const detailTitle = el('maintenanceDetailTitle');
  const detailList = el('maintenanceDetailList');

  if (!card || !title || !list) return;

  const lines = [
    maintenanceLine('Kaffeemaschine reinigen', adjustedMaintenanceSeconds('machine', m?.machine_seconds_to_due)),
    maintenanceLine('Mühle reinigen', adjustedMaintenanceSeconds('grinder', m?.grinder_seconds_to_due)),
    maintenanceLine('Filter wechseln', adjustedMaintenanceSeconds('filter', m?.filter_seconds_to_due))
  ];

  // Clientseitig aus den aktuell laufenden Sekundenwerten ableiten.
  // Dadurch erscheint der Warnhinweis auch dann sofort, wenn ein Countdown im geöffneten Browser auf 0 kippt.
  const dueCount = lines.filter(line => line.due).length;

  // Dashboard: nur anzeigen, wenn Wartung wirklich fällig ist.
  card.classList.toggle('show', dueCount > 0);
  card.classList.toggle('warn', dueCount > 0);
  card.classList.toggle('ok', false);

  if (!lastState?.time?.valid) {
    if (dueCount > 0) {
      title.textContent = dueCount === 1 ? 'Wartung: 1 Hinweis' : `Wartung: ${dueCount} Hinweise`;
      list.innerHTML = '<li class="maintenance-item due">Wartung erforderlich. Uhrzeit noch nicht synchronisiert.</li>';
    } else {
      title.textContent = 'Wartung: ok';
      list.innerHTML = '';
    }

    if (detailCard && detailTitle && detailList) {
      detailCard.classList.add('show');
      detailCard.classList.toggle('warn', dueCount > 0);
      detailCard.classList.toggle('ok', dueCount === 0);
      detailTitle.textContent = 'Wartungszeiten';
      detailList.innerHTML = '<li>Wartungszeiten werden angezeigt, sobald die Uhrzeit gültig ist.</li>';
    }
    return;
  }

  title.textContent = dueCount === 1 ? 'Wartung: 1 Hinweis' : `Wartung: ${dueCount} Hinweise`;
  list.innerHTML = lines
    .filter(line => line.due)
    .map(line => `<li class="maintenance-item due">${line.text}</li>`)
    .join('');

  // Einstellungen: immer alle Details anzeigen.
  if (detailCard && detailTitle && detailList) {
    detailCard.classList.add('show');
    detailCard.classList.toggle('warn', dueCount > 0);
    detailCard.classList.toggle('ok', dueCount === 0);
    detailTitle.textContent = dueCount === 0
      ? 'Wartungszeiten'
      : (dueCount === 1 ? 'Wartungszeiten: 1 Hinweis' : `Wartungszeiten: ${dueCount} Hinweise`);
    detailList.innerHTML = lines
      .map(line => `<li class="maintenance-item ${line.due ? 'due' : 'ok'}">${line.text}</li>`)
      .join('');
  }
}

// ===== Stoppuhr =====
function syncStopwatchTimer(sw) {
  const newMs = Math.max(0, Number(sw?.ms || 0));
  const newRunning = !!sw?.running;
  const nowMs = Date.now();

  const currentMs = stopwatchRunning
    ? stopwatchBaseMs + Math.max(0, nowMs - stopwatchBaseClientMs)
    : stopwatchBaseMs;

  if (newRunning !== stopwatchRunning || !stopwatchBaseClientMs || Math.abs(newMs - currentMs) > 1000 || !newRunning) {
    stopwatchBaseClientMs = nowMs;
    stopwatchBaseMs = newMs;
    stopwatchRunning = newRunning;
  }
}

function currentStopwatchMs() {
  if (!stopwatchRunning) return stopwatchBaseMs;
  return stopwatchBaseMs + Math.max(0, Date.now() - stopwatchBaseClientMs);
}

function renderStopwatch() {
  el('stopwatch').textContent = fmtStopwatchTime(currentStopwatchMs());
  el('swToggle').textContent = stopwatchRunning ? 'Stop' : 'Start';
}

// ===== Settings / Gefaesse =====
function renderGefaessSettings(s) {
  const list = el('gefaessList');
  if (!list) return;

  const weights = s?.gefaesse?.weights_g || [];
  list.innerHTML = GEFAESS_INDEXES.map(index => {
    const weight = Number(weights[index] || 0);
    const measured = weight > 0.05;
    const label = `Gefäß ${index + 1}`;
    const value = measured
      ? `<span class="gefaess-weight">${fmtG(weight)} g</span>`
      : '<span class="gefaess-missing">nicht eingemessen</span>';
    const button = measured
      ? `<button class="compact danger delete-gefaess" data-gefaess-index="${index}">löschen</button>`
      : '<button class="compact secondary" disabled>löschen</button>';
    return `<div class="gefaess-row"><div><b>${label}:</b> ${value}</div>${button}</div>`;
  }).join('');
}

// ===== State-Rendering =====
function renderWeightAndSelection(s) {
  setText('actual', fmtG(s.weight?.actual_g));
  if (!targetWeightDirty && document.activeElement !== el('targetWeight')) {
    el('targetWeight').value = fmtG(s.weight?.set_g);
  }
  setText('status', s.status?.label || String(s.status?.mode ?? '---'));
  el('siebtraegerSelect').value = String(s.selection?.siebtraeger ?? 0);
}

function renderAutodetect(s) {
  const autodetectOn = !!s.selection?.autodetect;
  const autodetectPaused = !!s.system?.autodetect_paused;
  const autodetectToggle = el('autodetectToggle');
  const autodetectLed = el('autodetectLed');

  setText('autodetectStatus', autodetectPaused ? 'pausiert' : (autodetectOn ? 'an' : 'aus'));
  autodetectToggle.classList.toggle('on', autodetectOn && !autodetectPaused);
  autodetectToggle.classList.toggle('off', !autodetectOn && !autodetectPaused);
  autodetectToggle.classList.toggle('paused', autodetectPaused);
  autodetectToggle.setAttribute('aria-pressed', autodetectOn ? 'true' : 'false');
  autodetectToggle.disabled = autodetectPaused;
  autodetectToggle.title = autodetectPaused ? 'Autodetect ist während des Assistenten pausiert' : (autodetectOn ? 'Autodetect ausschalten' : 'Autodetect einschalten');
  autodetectLed.classList.toggle('on', autodetectOn && !autodetectPaused);
}

function renderWifiBars(id, level, title) {
  const icon = el(id);
  if (!icon) return;
  icon.innerHTML = [0, 1, 2, 3].map(i => `<span class="bar${i < level ? ' on' : ''}"></span>`).join('');
  icon.setAttribute('title', title);
}

function renderWifiSignal(s) {
  const connected = !!s.system?.wifi;
  const level = connected ? Math.max(0, Math.min(4, Number(s.system?.wifi_signal_level ?? 0))) : 0;
  const label = connected ? (s.system?.wifi_signal_label || '---') : 'getrennt';
  const rssi = s.system?.wifi_rssi_dbm;
  const hasRssi = connected && Number.isFinite(Number(rssi));
  const rssiText = hasRssi ? `(${rssi} dBm)` : '';
  const title = connected ? `${label}${hasRssi ? ` (${rssi} dBm)` : ''}` : 'getrennt';

  renderWifiBars('wifiSignalIcon', level, title);
  renderWifiBars('wifiSignalHeaderIcon', level, title);
  renderWifiBars('statsWifiSignalIcon', level, title);

  setText('wifiSignalLabel', label);
  setText('wifiSignalRssi', rssiText);
  setText('statsWifiSignalLabel', label);
  setText('statsWifiSignalRssi', rssiText);
}

function renderStatsAndSystem(s) {
  setText('shotsTotal', s.stats?.shots?.total ?? 0);
  setText('shotsMachine', s.stats?.shots?.since_machine_clean ?? 0);
  setText('shotsGrinder', s.stats?.shots?.since_grinder_clean ?? 0);
  setText('shotsFilter', s.stats?.shots?.since_filter_change ?? 0);
  setText('groundTotalKg', fmtKg(s.stats?.ground?.total_g));
  setText('groundMachineG', fmtWholeG(s.stats?.ground?.since_machine_clean_g));
  setText('groundGrinderG', fmtWholeG(s.stats?.ground?.since_grinder_clean_g));
  setText('groundFilterG', fmtWholeG(s.stats?.ground?.since_filter_change_g));
  setText('statsTotalShotsView', s.stats?.shots?.total ?? 0);
  setText('statsTotalGroundView', `${fmtG(s.stats?.ground?.total_g)} g`);
  setText('datetime', fmtDateTime(s.time?.epoch, s.time?.valid));
  setText('uptime', fmtUptime(s.system?.uptime_ms));
  setText('ip', s.system?.ip || location.hostname);
  setText('statsWifiSsid', s.system?.wifi_ssid || '---');
  setText('wifiStatus', s.system?.wifi ? 'verbunden' : 'getrennt');
  setText('wifiSsid', s.system?.wifi_ssid || '---');
  renderWifiSignal(s);
  setText('wifiCredentialSource', s.system?.wifi_credential_source || '---');
  setText('wifiStoredCredentials', s.system?.wifi_stored_credentials ? 'ja' : 'nein');
  setText('wifiStoredCredentialsActive', s.system?.wifi_stored_credentials_active ? 'ja' : 'nein');
  setText('wifiSetupApStatus', s.system?.wifi_setup_ap_active ? 'an' : 'aus');
  setText('wifiSetupApAddress', s.system?.wifi_setup_ap_active ? `WLAN: ${s.system?.wifi_setup_ap_ssid || 'Waagen-Setup'} / ${s.system?.wifi_setup_ap_ip || '192.168.4.1'}` : '---');

  const storedSsid = s.system?.wifi_stored_ssid || '';
  const storedPassword = !!s.system?.wifi_stored_password;
  setText('wifiStoredSsid', storedSsid || '---');
  setText('wifiStoredPasswordStatus', storedPassword ? 'vorhanden' : 'nicht gespeichert');

  const ssidInput = el('wifiSetupSsid');
  if (ssidInput && document.activeElement !== ssidInput && !ssidInput.value && storedSsid) {
    ssidInput.value = storedSsid;
  }

  const passwordInput = el('wifiSetupPassword');
  if (passwordInput) {
    passwordInput.placeholder = storedPassword
      ? 'Leer lassen, um gespeichertes Passwort zu behalten'
      : 'WLAN-Passwort';
  }
}

function renderActionAvailability(s) {
  el('save').disabled = !s.status?.save_ready;
  el('swToggle').disabled = false;
  el('swReset').disabled = false;
}

function render(s) {
  lastState = s;
  syncMaintenanceTimers(s.maintenance);

  renderWeightAndSelection(s);
  renderAutodetect(s);
  syncStopwatchTimer(s.stopwatch);
  renderStopwatch();
  renderStatsAndSystem(s);
  renderActionAvailability(s);
  renderMaintenance(s.maintenance);
  renderGefaessSettings(s);

  if (el('wizardOverlay').classList.contains('show')) {
    updateWizardLiveFields();
  }
}
)rawliteral";

static const char COFFEE_EVENTS_JS[] PROGMEM = R"rawliteral(// ===== WebSocket-Verbindung =====
function connect() {
  el('ws').textContent = 'Verbinde…';
  el('ws').classList.remove('ok');
  const proto = location.protocol === 'https:' ? 'wss:' : 'ws:';
  ws = new WebSocket(`${proto}//${location.host}/ws`);
  ws.onopen = () => {
    el('ws').textContent = 'Verbunden';
    el('ws').classList.add('ok');
    el('swToggle').disabled = false;
    el('swReset').disabled = false;
    addLog('WebSocket verbunden');
  };
  ws.onclose = () => {
    el('ws').textContent = 'Getrennt';
    el('ws').classList.remove('ok');
    el('swToggle').disabled = true;
    el('swReset').disabled = true;
    addLog('WebSocket getrennt, reconnect läuft …');
    setTimeout(connect, 1500);
  };
  ws.onerror = () => { addLog('WebSocket-Fehler'); };
  ws.onmessage = e => {
    const data = JSON.parse(e.data);
    if (data.type === 'state') render(data);
    if (data.type === 'ack') addLog(`OK: ${data.cmd}`);
    if (data.type === 'error') addLog(`Fehler: ${data.cmd || ''} ${data.message}`);
  };
}

// ===== Event-Handler =====
function sendTargetWeight() {
  if (!isWebSocketReady()) return;
  const value = Number(el('targetWeight').value.replace(',', '.'));
  if (!Number.isFinite(value) || value <= 0 || value > 60.0) {
    addLog('Ungültiges Sollgewicht (erlaubt: 0,1 bis 60,0 g)');
    el('targetWeight').value = fmtG(currentState?.weight?.set_g ?? 0);
    return;
  }
  targetWeightDirty = false;
  el('targetWeight').value = value.toFixed(1);
  sendCommand(`set_selected_siebtraeger_weight_${value.toFixed(1)}`, `Sollgewicht gesendet: ${value.toFixed(1)} g`);
}

async function saveWifiCredentials(e) {
  e.preventDefault();

  const ssidInput = el('wifiSetupSsid');
  const passwordInput = el('wifiSetupPassword');
  const ssid = ssidInput.value.trim();
  const password = passwordInput.value;

  if (!ssid) {
    addLog('Bitte WLAN-Name/SSID eintragen');
    ssidInput.focus();
    return;
  }

  try {
    addLog('WLAN-Daten speichern …');
    const body = new URLSearchParams();
    body.set('ssid', ssid);
    body.set('password', password);

    const response = await fetch('/api/wifi/credentials', {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body
    });
    if (!response.ok) {
      addLog(`Fehler beim Speichern der WLAN-Daten: HTTP ${response.status}`);
      return;
    }
    const data = await response.json();
    addLog(data.message || 'WLAN-Daten gespeichert. Sie werden erst nach Aktivierung verwendet.');
    if (typeof data.stored_credentials === 'boolean') {
      setText('wifiStoredCredentials', data.stored_credentials ? 'ja' : 'nein');
    }
    if (typeof data.active === 'boolean') {
      setText('wifiStoredCredentialsActive', data.active ? 'ja' : 'nein');
    }
    if (typeof data.stored_ssid === 'string') {
      setText('wifiStoredSsid', data.stored_ssid || '---');
      if (data.stored_ssid) ssidInput.value = data.stored_ssid;
    }
    if (typeof data.stored_password === 'boolean') {
      setText('wifiStoredPasswordStatus', data.stored_password ? 'vorhanden' : 'nicht gespeichert');
      passwordInput.placeholder = data.stored_password
        ? 'Leer lassen, um gespeichertes Passwort zu behalten'
        : 'WLAN-Passwort';
    }
    passwordInput.value = '';
  } catch (err) {
    addLog('Fehler beim Speichern der WLAN-Daten');
  }
}

async function clearWifiCredentials() {
  try {
    addLog('Gespeicherte WLAN-Daten löschen …');
    const response = await fetch('/api/wifi/credentials', { method: 'DELETE' });
    if (!response.ok) {
      addLog(`Fehler beim Löschen der gespeicherten WLAN-Daten: HTTP ${response.status}`);
      return;
    }
    const data = await response.json();
    addLog(data.message || 'Gespeicherte WLAN-Daten gelöscht. Bitte ESP32 neu starten.');
    if (typeof data.stored_credentials === 'boolean') {
      setText('wifiStoredCredentials', data.stored_credentials ? 'ja' : 'nein');
    }
    if (typeof data.active === 'boolean') {
      setText('wifiStoredCredentialsActive', data.active ? 'ja' : 'nein');
    }
    if (typeof data.stored_ssid === 'string') {
      setText('wifiStoredSsid', data.stored_ssid || '---');
      el('wifiSetupSsid').value = data.stored_ssid || '';
    }
    if (typeof data.stored_password === 'boolean') {
      setText('wifiStoredPasswordStatus', data.stored_password ? 'vorhanden' : 'nicht gespeichert');
      el('wifiSetupPassword').placeholder = data.stored_password
        ? 'Leer lassen, um gespeichertes Passwort zu behalten'
        : 'WLAN-Passwort';
    }
  } catch (err) {
    addLog('Fehler beim Löschen der gespeicherten WLAN-Daten');
  }
}

async function setWifiCredentialsActive(active) {
  try {
    addLog(active ? 'Gespeicherte WLAN-Daten verwenden …' : 'Standard-WLAN verwenden …');
    const response = await fetch(active ? '/api/wifi/credentials/activate' : '/api/wifi/credentials/deactivate', { method: 'POST' });
    if (!response.ok) {
      addLog(`Fehler beim Ändern der WLAN-Auswahl: HTTP ${response.status}`);
      return;
    }
    const data = await response.json();
    addLog(data.message || 'WLAN-Auswahl geändert. Bitte ESP32 neu starten.');
    if (typeof data.stored_credentials === 'boolean') {
      setText('wifiStoredCredentials', data.stored_credentials ? 'ja' : 'nein');
    }
    if (typeof data.active === 'boolean') {
      setText('wifiStoredCredentialsActive', data.active ? 'ja' : 'nein');
    }
    if (typeof data.stored_ssid === 'string') {
      setText('wifiStoredSsid', data.stored_ssid || '---');
    }
    if (typeof data.stored_password === 'boolean') {
      setText('wifiStoredPasswordStatus', data.stored_password ? 'vorhanden' : 'nicht gespeichert');
    }
  } catch (err) {
    addLog('Fehler beim Ändern der WLAN-Auswahl');
  }
}

async function setWifiSetupApEnabled(enabled) {
  try {
    addLog(enabled ? 'Setup-WLAN starten …' : 'Setup-WLAN stoppen …');
    const response = await fetch(enabled ? '/api/wifi/setup-ap/start' : '/api/wifi/setup-ap/stop', { method: 'POST' });
    if (!response.ok) {
      const message = `Fehler beim ${enabled ? 'Starten' : 'Stoppen'} des Setup-WLANs: HTTP ${response.status}`;
      addLog(message);
      openInfoOverlay('Setup-WLAN Fehler', message);
      return;
    }
    const data = await response.json();
    addLog(data.message || (enabled ? 'Setup-WLAN gestartet.' : 'Setup-WLAN gestoppt.'));
    if (typeof data.active === 'boolean') {
      const ssid = data.ssid || 'Waagen-Setup';
      const ip = data.ip || '192.168.4.1';
      setText('wifiSetupApStatus', data.active ? 'an' : 'aus');
      setText('wifiSetupApAddress', data.active ? `WLAN: ${ssid} / ${ip}` : '---');
      if (data.active) {
        openInfoOverlay('Setup-WLAN gestartet', `WLAN: ${ssid}
Adresse: ${ip}
Normale WebUI bleibt im Heim-WLAN erreichbar.`);
      } else {
        openInfoOverlay('Setup-WLAN gestoppt', 'Das Setup-WLAN wurde beendet. Die normale WebUI bleibt im Heim-WLAN erreichbar.');
      }
    }
  } catch (err) {
    const message = enabled ? 'Fehler beim Starten des Setup-WLANs' : 'Fehler beim Stoppen des Setup-WLANs';
    addLog(message);
    openInfoOverlay('Setup-WLAN Fehler', message);
  }
}

function handleSaveClick() {
  if (lastState?.status?.save_ready) {
    sendCommand(CMD.saveDose, 'Save gesendet …');
  }
}

function handleStopwatchToggleClick() {
  sendCommand(CMD.stopwatchStartStop, stopwatchRunning ? 'Stoppuhr Stop gesendet …' : 'Stoppuhr Start gesendet …');
}

function handleSiebtraegerChange() {
  const idx = Number(el('siebtraegerSelect').value);
  sendCommand(`select_siebtraeger_${idx}`, `Siebträger-Auswahl gesendet: ${idx}`);
}

function handleAutodetectToggleClick() {
  const autodetectOn = !!lastState?.selection?.autodetect;
  sendCommand(autodetectOn ? CMD.autodetectOff : CMD.autodetectOn, autodetectOn ? 'Autodetect aus gesendet …' : 'Autodetect an gesendet …');
}

function handleGefaessListClick(e) {
  const button = e.target.closest('.delete-gefaess');
  if (!button) return;

  const index = Number(button.dataset.gefaessIndex);
  if (!Number.isInteger(index) || index < 0 || index > 3) return;

  openConfirmOverlay(
    `Gefäß ${index + 1} löschen?`,
    `Das gespeicherte Gewicht für Gefäß ${index + 1} wird gelöscht. Autodetect erkennt dieses Gefäß danach nicht mehr.`,
    `delete_gefaess_${index}`,
    `Gefäß ${index + 1} löschen gesendet …`
  );
}

function handleGlobalKeyDown(e) {
  if (el('confirmOverlay').classList.contains('show')) {
    if (e.key === 'Escape') {
      e.preventDefault();
      closeConfirmOverlay();
    }
    if (e.key === 'Enter') {
      e.preventDefault();
      confirmPendingAction();
    }
    return;
  }

  if (el('wizardOverlay').classList.contains('show') && e.key === 'Escape') {
    e.preventDefault();
    closeWizardOverlay();
  }
}

function handleTargetWeightKeyDown(e) {
  if (e.key === 'Enter') {
    e.preventDefault();
    sendTargetWeight();
  }
}

function bindDashboardHandlers() {
  el('save').addEventListener('click', handleSaveClick);
  el('tare').addEventListener('click', () => sendCommand(CMD.tare, 'Tara gesendet …'));
  el('swToggle').addEventListener('click', handleStopwatchToggleClick);
  el('swReset').addEventListener('click', () => sendCommand(CMD.stopwatchReset, 'Stoppuhr Reset gesendet …'));
  el('siebtraegerSelect').addEventListener('change', handleSiebtraegerChange);
  el('targetSave').addEventListener('click', sendTargetWeight);
  el('targetWeight').addEventListener('input', () => { targetWeightDirty = true; });
  el('targetWeight').addEventListener('keydown', handleTargetWeightKeyDown);
  el('autodetectToggle').addEventListener('click', handleAutodetectToggleClick);
}

function bindNavigationHandlers() {
  document.querySelectorAll('.tab-button').forEach(button => {
    button.addEventListener('click', () => showTab(button.dataset.tab));
  });
  document.querySelectorAll('.settings-tab-button').forEach(button => {
    button.addEventListener('click', () => showSettingsTab(button.dataset.settingsTab));
  });
}

function bindSettingsHandlers() {
  el('openCalibrate').addEventListener('click', openCalibrationWizard);
  el('openMeasureGefaess').addEventListener('click', openMeasureGefaessWizard);
  el('openStatsTotals').addEventListener('click', openStatsTotalsWizard);
  el('wifiCredentialsForm').addEventListener('submit', saveWifiCredentials);
  el('activateWifiCredentials').addEventListener('click', () => openConfirmOverlay(
    'Gespeicherte WLAN-Daten verwenden?',
    'Beim nächsten Neustart versucht die Waage, die gespeicherten WLAN-Daten zu verwenden. Wenn das fehlschlägt, nutzt sie automatisch wieder das Standard-WLAN aus der Firmware.',
    'activate_wifi_credentials_webui',
    'Gespeicherte WLAN-Daten verwenden …'
  ));
  el('deactivateWifiCredentials').addEventListener('click', () => openConfirmOverlay(
    'Standard-WLAN verwenden?',
    'Beim nächsten Neustart nutzt die Waage wieder das Standard-WLAN aus der Firmware. Gespeicherte WLAN-Daten bleiben erhalten.',
    'deactivate_wifi_credentials_webui',
    'Standard-WLAN verwenden …'
  ));
  el('clearWifiCredentials').addEventListener('click', () => openConfirmOverlay(
    'Gespeicherte WLAN-Daten löschen?',
    'Gespeicherte WLAN-Daten werden gelöscht. Die aktuelle Verbindung bleibt bis zum Neustart unverändert.',
    'clear_wifi_credentials_webui',
    'Gespeicherte WLAN-Daten löschen …'
  ));
  el('startWifiSetupAp').addEventListener('click', () => setWifiSetupApEnabled(true));
  el('stopWifiSetupAp').addEventListener('click', () => setWifiSetupApEnabled(false));
  el('openUpdatePage').addEventListener('click', () => { window.location.href = '/update'; });
  el('restartDevice').addEventListener('click', () => openConfirmOverlay(
    'ESP32 wirklich neu starten?',
    'Der ESP32 startet neu. Die WebUI ist während des Neustarts kurz nicht erreichbar.',
    CMD.restartDevice,
    'Neustart angefordert …'
  ));
  el('resetMachine').addEventListener('click', () => openConfirmOverlay(
    'Kaffeemaschinenreinigung reset?',
    'Dadurch werden Zeitpunkt und Zähler seit der letzten Kaffeemaschinenreinigung zurückgesetzt.',
    CMD.maintenanceResetMachine,
    'Reset Kaffeemaschine gesendet …'
  ));
  el('resetGrinder').addEventListener('click', () => openConfirmOverlay(
    'Mühlenreinigung reset?',
    'Dadurch werden Zeitpunkt und Zähler seit der letzten Mühlenreinigung zurückgesetzt.',
    CMD.maintenanceResetGrinder,
    'Reset Mühle gesendet …'
  ));
  el('resetFilter').addEventListener('click', () => openConfirmOverlay(
    'Filterwechsel reset?',
    'Dadurch werden Zeitpunkt und Zähler seit dem letzten Filterwechsel zurückgesetzt.',
    CMD.maintenanceResetFilter,
    'Reset Filter gesendet …'
  ));
  el('gefaessList').addEventListener('click', handleGefaessListClick);
}

function bindOverlayHandlers() {
  el('confirmCancel').addEventListener('click', closeConfirmOverlay);
  el('confirmOk').addEventListener('click', confirmPendingAction);
  el('confirmOverlay').addEventListener('click', e => {
    if (e.target === el('confirmOverlay')) closeConfirmOverlay();
  });
  el('wizardOverlay').addEventListener('click', e => {
    if (e.target === el('wizardOverlay')) closeWizardOverlay();
  });
  document.addEventListener('keydown', handleGlobalKeyDown);
}

function startClientTimers() {
  setInterval(function maintenanceClientTick() {
    if (!lastState) return;
    renderMaintenance(lastState.maintenance);
  }, 1000);

  setInterval(function stopwatchClientTick() {
    renderStopwatch();
  }, 100);
}

function initWebUi() {
  bindDashboardHandlers();
  bindNavigationHandlers();
  bindSettingsHandlers();
  bindOverlayHandlers();
  startClientTimers();
  setTimeout(connect, 750);
}

window.addEventListener('beforeunload', () => {
  if (isWebSocketReady() && !wizardEndSent) {
    ws.send(JSON.stringify({ cmd: CMD.wizardEnd }));
  }
});

initWebUi();
)rawliteral";

static const char PWA_MANIFEST_JSON[] PROGMEM = R"rawliteral({
  "name": "Kaffeewaage",
  "short_name": "Waage",
  "start_url": "/?source=pwa",
  "scope": "/",
  "display": "standalone",
  "background_color": "#111827",
  "theme_color": "#92400e",
  "icons": [
    {
      "src": "/icon-192.png?v=1",
      "sizes": "192x192",
      "type": "image/png"
    },
    {
      "src": "/icon-512.png?v=1",
      "sizes": "512x512",
      "type": "image/png"
    }
  ]
})rawliteral";

static const char WIFI_SETUP_HTML[] PROGMEM = R"rawliteral(<!doctype html>
<html lang="de">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>WLAN einrichten</title>
  <style>
    :root { font-family: system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif; color: #f4f7fb; background: #061018; }
    body { margin: 0; min-height: 100vh; padding: 18px; background: linear-gradient(180deg, #0b111b 0%, #061018 100%); }
    main { max-width: 520px; margin: 0 auto; display: grid; gap: 14px; }
    .card { background: #101b27; border: 1px solid rgba(148,163,184,.24); border-radius: 22px; padding: 18px; box-shadow: 0 18px 48px rgba(0,0,0,.32); }
    h1 { margin: 0 0 8px; font-size: 1.35rem; }
    p { color: #c4ccd8; line-height: 1.45; }
    label { display: grid; gap: 6px; margin: 12px 0; color: #9aa8b8; }
    input { box-sizing: border-box; width: 100%; border: 1px solid rgba(148,163,184,.38); border-radius: 12px; padding: 11px 12px; font: inherit; background: #0b1520; color: #f4f7fb; }
    button { width: 100%; border: 1px solid rgba(85,195,66,.36); border-radius: 16px; padding: 14px; font-size: 1rem; font-weight: 750; background: linear-gradient(180deg, #55c342, #3fa72d); color: #f8fff8; cursor: pointer; }
    button.secondary { margin-top: 10px; background: #1a2533; border-color: rgba(85,195,66,.46); }
    .log { margin-top: 12px; padding: 10px 12px; border-radius: 14px; background: #0b1520; border: 1px solid rgba(148,163,184,.24); color: #c4ccd8; min-height: 1.2em; }
    .hint { font-size: .92rem; color: #9aa8b8; }
    .modal-backdrop { position: fixed; inset: 0; display: none; place-items: center; padding: 18px; background: rgba(2,6,12,.72); z-index: 20; }
    .modal-backdrop.show { display: grid; }
    .modal { width: min(420px, 100%); background: #101b27; border: 1px solid rgba(148,163,184,.24); border-radius: 22px; padding: 20px; box-shadow: 0 24px 70px rgba(0,0,0,.48); }
    .modal-title { font-size: 1.2rem; font-weight: 800; margin-bottom: 8px; }
    .modal-text { color: #c4ccd8; white-space: pre-line; margin-bottom: 16px; }
  </style>
</head>
<body>
<main>
  <section class="card">
    <h1>WLAN einrichten</h1>
    <p>Verbunden mit <b>Waagen-Setup</b>. Trage hier das WLAN ein, mit dem sich die Waage künftig verbinden soll.</p>
    <form id="wifiSetupForm" autocomplete="off">
      <label>SSID / WLAN-Name<input id="ssid" name="ssid" type="text" required placeholder="WLAN-Name"></label>
      <label>Passwort<input id="password" name="password" type="password" autocomplete="new-password" placeholder="WLAN-Passwort"></label>
      <button type="submit">Speichern und verwenden</button>
    </form>
    <button id="reboot" class="secondary" type="button">ESP32 neu starten</button>
    <p class="hint">Nach dem Speichern werden die Daten aktiviert. Starte die Waage danach neu. Wenn die Verbindung scheitert, nutzt sie automatisch wieder das Standard-WLAN aus der Firmware.</p>
    <div id="log" class="log"></div>
  </section>
</main>
<div id="infoOverlay" class="modal-backdrop" role="dialog" aria-modal="true" aria-labelledby="infoTitle">
  <div class="modal">
    <div class="modal-title" id="infoTitle">Hinweis</div>
    <div class="modal-text" id="infoText"></div>
    <button id="infoOk" type="button">OK</button>
  </div>
</div>
<script>
const log = document.getElementById('log');
function openInfoOverlay(title, text) {
  document.getElementById('infoTitle').textContent = title;
  document.getElementById('infoText').textContent = text;
  document.getElementById('infoOverlay').classList.add('show');
}
document.getElementById('infoOk').addEventListener('click', () => {
  document.getElementById('infoOverlay').classList.remove('show');
});
document.getElementById('wifiSetupForm').addEventListener('submit', async (e) => {
  e.preventDefault();
  const body = new URLSearchParams();
  body.set('ssid', document.getElementById('ssid').value.trim());
  body.set('password', document.getElementById('password').value);
  log.textContent = 'Speichere WLAN-Daten …';
  try {
    const res = await fetch('/api/wifi/setup', { method: 'POST', headers: { 'Content-Type': 'application/x-www-form-urlencoded' }, body });
    const data = await res.json();
    const message = data.message || (res.ok ? 'Gespeichert. Bitte ESP32 neu starten.' : 'Fehler beim Speichern.');
    log.textContent = message;
    if (res.ok) {
      openInfoOverlay('WLAN-Daten gespeichert', 'Die WLAN-Daten wurden gespeichert und aktiviert.\nBitte starte die Waage neu.');
    }
  } catch (err) {
    log.textContent = 'Fehler beim Speichern der WLAN-Daten.';
  }
});
document.getElementById('reboot').addEventListener('click', async () => {
  log.textContent = 'Neustart angefordert …';
  try { await fetch('/update/reboot', { method: 'POST' }); } catch (err) {}
});
</script>
</body>
</html>)rawliteral";


// =============================================================================
// C++: HTTP / WebSocket / State-Serialisierung
// =============================================================================

static const char* appStatusLabel(AppStatusMode mode)
{
  switch (mode) {
    case APP_STATUS_IDLE: return "idle";
    case APP_STATUS_MEASURING: return "measuring";
    case APP_STATUS_STABLE: return "stable";
    case APP_STATUS_SAVE_READY: return "save_ready";
    default: return "unknown";
  }
}

static bool isSetupApHost(AsyncWebServerRequest* request)
{
  if (!request || !coffeeWifiSetupApActive()) {
    return false;
  }

  const String host = request->host();
  const String setupIp = coffeeWifiSetupApIp();
  return setupIp.length() > 0 && host.startsWith(setupIp);
}

static void sendProgmemResponse(AsyncWebServerRequest* request,
                                const char* content,
                                const char* contentType,
                                const char* cacheControl)
{
  if (!request || !content || !contentType) {
    return;
  }

  const size_t totalLen = strlen_P(content);
  AsyncWebServerResponse* response = request->beginResponse_P(
    200,
    contentType,
    reinterpret_cast<const uint8_t*>(content),
    totalLen);

  if (cacheControl && cacheControl[0] != '\0') {
    response->addHeader("Cache-Control", cacheControl);
  }

  // Auf dem T4-S3/LVGL-Zweig zeigte die chunked PROGMEM-Auslieferung lange
  // Time-to-first-byte-Werte und teils abgebrochene Transfers. Die Dateien sind
  // statisch und ihre Laenge ist bekannt, daher ist eine normale PROGMEM-
  // Response mit Content-Length hier robuster als Transfer-Encoding: chunked.
  // Kein erzwungenes "Connection: close": der Browser darf TCP-Verbindungen
  // wiederverwenden. Das vermeidet mehrere neue Verbindungsaufbauten fuer die
  // CSS/JS-Dateien, was bei schwachem WLAN spuerbar schneller ist.
  request->send(response);
}

static void sendProgmemHtmlResponse(AsyncWebServerRequest* request, const char* html)
{
  // Die WebUI wird direkt aus dem Firmware-Image geliefert. Nach OTA- oder
  // WLAN-Setup-Aenderungen soll der Browser das HTML sicher neu laden.
  sendProgmemResponse(request, html, "text/html", "no-store");
}

void coffeeWebHandleRoot(AsyncWebServerRequest* request)
{
  if (!request) {
    return;
  }

  if (isSetupApHost(request)) {
    sendProgmemHtmlResponse(request, WIFI_SETUP_HTML);
    return;
  }

  sendProgmemHtmlResponse(request, INDEX_HTML);
}


// -----------------------------------------------------------------------------
// WebSocket-Antworten und Eingangsverarbeitung
// -----------------------------------------------------------------------------

static void sendWsError(AsyncWebSocketClient* client, const char* cmd, const char* message)
{
  StaticJsonDocument<192> doc;
  doc["type"] = "error";
  if (cmd && cmd[0] != '\0') {
    doc["cmd"] = cmd;
  }
  doc["message"] = message;

  String out;
  serializeJson(doc, out);
  client->text(out);
}

static void sendWsAck(AsyncWebSocketClient* client, const char* cmd)
{
  StaticJsonDocument<128> doc;
  doc["type"] = "ack";
  doc["cmd"] = cmd;

  String out;
  serializeJson(doc, out);
  client->text(out);
}

static void handleWsText(AsyncWebSocketClient* client, const uint8_t* data, size_t len)
{
  if (!client || !data || len == 0) {
    return;
  }

  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, data, len);
  if (err) {
    sendWsError(client, "", "invalid_json");
    return;
  }

  const char* cmd = doc["cmd"] | "";
  if (cmd[0] == '\0') {
    sendWsError(client, "", "missing_cmd");
    return;
  }

  if (!commandHandler) {
    sendWsError(client, cmd, "command_handler_missing");
    return;
  }

  if (commandHandler(cmd)) {
    sendWsAck(client, cmd);
    return;
  }

  sendWsError(client, cmd, "command_failed");
}

// -----------------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------------

void coffeeWebBegin(AsyncWebServer& server)
{
  ws.onEvent([](AsyncWebSocket *server,
                AsyncWebSocketClient *client,
                AwsEventType type,
                void *arg,
                uint8_t *data,
                size_t len)
  {
    if (type == WS_EVT_CONNECT) {
      Serial.println("[WS] Client connected");
      return;
    }

    if (type == WS_EVT_DATA) {
      AwsFrameInfo* info = static_cast<AwsFrameInfo*>(arg);
      if (info && info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
        handleWsText(client, data, len);
      }
      return;
    }
  });

  server.on("/wifi-setup", HTTP_GET, [](AsyncWebServerRequest* request) {
    sendProgmemHtmlResponse(request, WIFI_SETUP_HTML);
  });

  server.on("/coffee.css", HTTP_GET, [](AsyncWebServerRequest* request) {
    // Versionierung erfolgt per Query-String im HTML-Link.
    sendProgmemResponse(request, COFFEE_CSS, "text/css", "public, max-age=31536000, immutable");
  });

  server.on("/coffee_core.js", HTTP_GET, [](AsyncWebServerRequest* request) {
    // Versionierung erfolgt per Query-String im HTML-Link.
    sendProgmemResponse(request, COFFEE_CORE_JS, "application/javascript", "public, max-age=31536000, immutable");
  });

  server.on("/coffee_render.js", HTTP_GET, [](AsyncWebServerRequest* request) {
    // Versionierung erfolgt per Query-String im HTML-Link.
    sendProgmemResponse(request, COFFEE_RENDER_JS, "application/javascript", "public, max-age=31536000, immutable");
  });

  server.on("/coffee_events.js", HTTP_GET, [](AsyncWebServerRequest* request) {
    // Versionierung erfolgt per Query-String im HTML-Link.
    sendProgmemResponse(request, COFFEE_EVENTS_JS, "application/javascript", "public, max-age=31536000, immutable");
  });

  server.on("/api/wifi/setup-ap/start", HTTP_POST, [](AsyncWebServerRequest* request) {
    const bool ok = coffeeWifiStartSetupAp();
    StaticJsonDocument<192> doc;
    doc["ok"] = ok;
    doc["message"] = ok ? "Setup-WLAN gestartet: Waagen-Setup / 192.168.4.1" : "Setup-WLAN konnte nicht gestartet werden.";
    doc["active"] = coffeeWifiSetupApActive();
    doc["ssid"] = coffeeWifiSetupApSsid();
    doc["ip"] = coffeeWifiSetupApIp();

    String out;
    serializeJson(doc, out);
    request->send(ok ? 200 : 500, "application/json", out);
  });

  server.on("/api/wifi/setup-ap/stop", HTTP_POST, [](AsyncWebServerRequest* request) {
    const bool ok = coffeeWifiStopSetupAp();
    StaticJsonDocument<192> doc;
    doc["ok"] = ok;
    doc["message"] = ok ? "Setup-WLAN gestoppt." : "Setup-WLAN konnte nicht gestoppt werden.";
    doc["active"] = coffeeWifiSetupApActive();
    doc["ssid"] = coffeeWifiSetupApSsid();
    doc["ip"] = coffeeWifiSetupApIp();

    String out;
    serializeJson(doc, out);
    request->send(ok ? 200 : 500, "application/json", out);
  });

  server.on("/api/wifi/setup", HTTP_POST, [](AsyncWebServerRequest* request) {
    if (!request->hasParam("ssid", true)) {
      StaticJsonDocument<160> doc;
      doc["ok"] = false;
      doc["message"] = "SSID fehlt.";

      String out;
      serializeJson(doc, out);
      request->send(400, "application/json", out);
      return;
    }

    const String ssid = request->getParam("ssid", true)->value();
    const String password = request->hasParam("password", true)
                              ? request->getParam("password", true)->value()
                              : String();
    const bool saved = coffeeWifiSaveCredentials(ssid, password);
    const bool activated = saved && coffeeWifiSetStoredCredentialsActive(true);

    StaticJsonDocument<224> doc;
    doc["ok"] = saved && activated;
    doc["message"] = (saved && activated)
                       ? "WLAN-Daten gespeichert und aktiviert. Bitte ESP32 neu starten."
                       : "WLAN-Daten konnten nicht gespeichert oder aktiviert werden.";
    doc["stored_credentials"] = coffeeWifiHasStoredCredentials();
    doc["active"] = coffeeWifiStoredCredentialsAreActive();
    doc["stored_ssid"] = coffeeWifiStoredSsid();
    doc["stored_password"] = coffeeWifiStoredPasswordAvailable();

    String out;
    serializeJson(doc, out);
    request->send((saved && activated) ? 200 : 400, "application/json", out);
  });

  server.on("/api/wifi/credentials", HTTP_POST, [](AsyncWebServerRequest* request) {
    // AsyncWebServer matched this handler also for the longer URLs
    // /api/wifi/credentials/activate and /api/wifi/credentials/deactivate on the device.
    // Dispatch those cases explicitly before treating the request as a credentials-save call.
    const String requestUrl = request->url();
    if (requestUrl.endsWith("/activate")) {
      const bool ok = coffeeWifiSetStoredCredentialsActive(true);
      StaticJsonDocument<224> doc;
      doc["ok"] = ok;
      doc["message"] = ok
                         ? "Gespeicherte WLAN-Daten werden beim nächsten Neustart verwendet."
                         : "Gespeicherte WLAN-Daten konnten nicht aktiviert werden. Sind Daten gespeichert?";
      doc["stored_credentials"] = coffeeWifiHasStoredCredentials();
      doc["active"] = coffeeWifiStoredCredentialsAreActive();
      doc["stored_ssid"] = coffeeWifiStoredSsid();
      doc["stored_password"] = coffeeWifiStoredPasswordAvailable();

      String out;
      serializeJson(doc, out);
      request->send(ok ? 200 : 400, "application/json", out);
      return;
    }

    if (requestUrl.endsWith("/deactivate")) {
      const bool ok = coffeeWifiSetStoredCredentialsActive(false);
      StaticJsonDocument<224> doc;
      doc["ok"] = ok;
      doc["message"] = ok
                         ? "Standard-WLAN wird beim nächsten Neustart verwendet."
                         : "Standard-WLAN konnte nicht aktiviert werden.";
      doc["stored_credentials"] = coffeeWifiHasStoredCredentials();
      doc["active"] = coffeeWifiStoredCredentialsAreActive();
      doc["stored_ssid"] = coffeeWifiStoredSsid();
      doc["stored_password"] = coffeeWifiStoredPasswordAvailable();

      String out;
      serializeJson(doc, out);
      request->send(ok ? 200 : 500, "application/json", out);
      return;
    }

    if (!request->hasParam("ssid", true)) {
      StaticJsonDocument<160> doc;
      doc["ok"] = false;
      doc["message"] = "SSID fehlt.";

      String out;
      serializeJson(doc, out);
      request->send(400, "application/json", out);
      return;
    }

    const String ssid = request->getParam("ssid", true)->value();
    const String password = request->hasParam("password", true)
                              ? request->getParam("password", true)->value()
                              : String();
    const bool ok = coffeeWifiSaveCredentials(ssid, password);

    StaticJsonDocument<224> doc;
    doc["ok"] = ok;
    doc["message"] = ok
                       ? "WLAN-Daten gespeichert. Sie werden erst nach Aktivierung verwendet."
                       : "WLAN-Daten konnten nicht gespeichert werden.";
    doc["stored_credentials"] = coffeeWifiHasStoredCredentials();
    doc["active"] = coffeeWifiStoredCredentialsAreActive();
    doc["stored_ssid"] = coffeeWifiStoredSsid();
    doc["stored_password"] = coffeeWifiStoredPasswordAvailable();

    String out;
    serializeJson(doc, out);
    request->send(ok ? 200 : 400, "application/json", out);
  });

  server.on("/api/wifi/credentials/activate", HTTP_POST, [](AsyncWebServerRequest* request) {
    const bool ok = coffeeWifiSetStoredCredentialsActive(true);
    StaticJsonDocument<224> doc;
    doc["ok"] = ok;
    doc["message"] = ok
                       ? "Gespeicherte WLAN-Daten werden beim nächsten Neustart verwendet."
                       : "Gespeicherte WLAN-Daten konnten nicht aktiviert werden. Sind Daten gespeichert?";
    doc["stored_credentials"] = coffeeWifiHasStoredCredentials();
    doc["active"] = coffeeWifiStoredCredentialsAreActive();
    doc["stored_ssid"] = coffeeWifiStoredSsid();
    doc["stored_password"] = coffeeWifiStoredPasswordAvailable();

    String out;
    serializeJson(doc, out);
    request->send(ok ? 200 : 400, "application/json", out);
  });

  server.on("/api/wifi/credentials/deactivate", HTTP_POST, [](AsyncWebServerRequest* request) {
    const bool ok = coffeeWifiSetStoredCredentialsActive(false);
    StaticJsonDocument<224> doc;
    doc["ok"] = ok;
    doc["message"] = ok
                       ? "Standard-WLAN wird beim nächsten Neustart verwendet."
                       : "Standard-WLAN konnte nicht aktiviert werden.";
    doc["stored_credentials"] = coffeeWifiHasStoredCredentials();
    doc["active"] = coffeeWifiStoredCredentialsAreActive();
    doc["stored_ssid"] = coffeeWifiStoredSsid();
    doc["stored_password"] = coffeeWifiStoredPasswordAvailable();

    String out;
    serializeJson(doc, out);
    request->send(ok ? 200 : 500, "application/json", out);
  });

  server.on("/api/wifi/credentials", HTTP_DELETE, [](AsyncWebServerRequest* request) {
    const bool ok = coffeeWifiClearCredentials();
    StaticJsonDocument<192> doc;
    doc["ok"] = ok;
    doc["message"] = ok ? "Gespeicherte WLAN-Daten gelöscht. Bitte ESP32 neu starten." : "WLAN-Speicher nicht erreichbar.";
    doc["stored_credentials"] = coffeeWifiHasStoredCredentials();
    doc["active"] = coffeeWifiStoredCredentialsAreActive();
    doc["stored_ssid"] = coffeeWifiStoredSsid();
    doc["stored_password"] = coffeeWifiStoredPasswordAvailable();

    String out;
    serializeJson(doc, out);
    request->send(ok ? 200 : 500, "application/json", out);
  });

  server.on("/manifest.json", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send_P(200, "application/manifest+json", PWA_MANIFEST_JSON);
  });

  server.on("/icon-192.png", HTTP_GET, [](AsyncWebServerRequest* request) {
    sendOptionalPwaAssetEmpty(request, "image/png");
  });

  server.on("/icon-512.png", HTTP_GET, [](AsyncWebServerRequest* request) {
    sendOptionalPwaAssetEmpty(request, "image/png");
  });

  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest* request) {
    sendOptionalPwaAssetEmpty(request, "image/x-icon");
  });

  server.addHandler(&ws);
}

void coffeeWebLoop()
{
  ws.cleanupClients();
}

void coffeeWebSetCommandHandler(CoffeeWebCommandHandler handler)
{
  commandHandler = handler;
}

// -----------------------------------------------------------------------------
// AppState -> WebSocket-State JSON
// -----------------------------------------------------------------------------

static String buildStateJson(const AppState& s)
{
  StaticJsonDocument<3072> doc;

  doc["type"] = "state";

  doc["weight"]["actual_g"] = s.weight.actual_g;
  doc["weight"]["set_g"] = s.weight.set_g;
  doc["weight"]["stable"] = s.weight.stable;

  doc["status"]["mode"] = s.status.mode;
  doc["status"]["label"] = appStatusLabel(s.status.mode);
  doc["status"]["save_ready"] = s.status.save_ready;

  doc["stopwatch"]["ms"] = s.stopwatch.ms;
  doc["stopwatch"]["running"] = s.stopwatch.running;

  doc["selection"]["siebtraeger"] = s.selection.siebtraeger;
  doc["selection"]["gefaess"] = s.selection.gefaess;
  doc["selection"]["autodetect"] = s.selection.autodetect;

  doc["calibration"]["set_weight_g"] = s.calibration.set_weight_g;
  doc["calibration"]["factor"] = s.calibration.factor;

  JsonArray gefaessWeights = doc["gefaesse"]["weights_g"].to<JsonArray>();
  for (int i = 0; i < 4; ++i) {
    gefaessWeights.add(s.gefaesse.weights_g[i]);
  }

  doc["stats"]["ground"]["total_g"] = s.stats.ground.total_g;
  doc["stats"]["ground"]["since_grinder_clean_g"] = s.stats.ground.since_grinder_clean_g;
  doc["stats"]["ground"]["since_machine_clean_g"] = s.stats.ground.since_machine_clean_g;
  doc["stats"]["ground"]["since_filter_change_g"] = s.stats.ground.since_filter_change_g;

  doc["stats"]["shots"]["total"] = s.stats.shots.total;
  doc["stats"]["shots"]["since_grinder_clean"] = s.stats.shots.since_grinder_clean;
  doc["stats"]["shots"]["since_machine_clean"] = s.stats.shots.since_machine_clean;
  doc["stats"]["shots"]["since_filter_change"] = s.stats.shots.since_filter_change;

  doc["maintenance"]["grinder_clean_due"] = s.maintenance.grinder_clean_due;
  doc["maintenance"]["machine_clean_due"] = s.maintenance.machine_clean_due;
  doc["maintenance"]["filter_change_due"] = s.maintenance.filter_change_due;
  doc["maintenance"]["grinder_seconds_to_due"] = s.maintenance.grinder_seconds_to_due;
  doc["maintenance"]["machine_seconds_to_due"] = s.maintenance.machine_seconds_to_due;
  doc["maintenance"]["filter_seconds_to_due"] = s.maintenance.filter_seconds_to_due;
  doc["maintenance"]["due_count"] = s.maintenance.due_count;

  doc["time"]["valid"] = s.time.valid;
  doc["time"]["epoch"] = s.time.epoch;

  doc["system"]["wifi"] = s.system.wifi_connected;
  doc["system"]["ip"] = s.system.ip;
  doc["system"]["wifi_rssi_dbm"] = s.system.wifi_rssi_dbm;
  doc["system"]["wifi_signal_level"] = s.system.wifi_signal_level;
  doc["system"]["wifi_signal_label"] = s.system.wifi_signal_label;
  doc["system"]["uptime_ms"] = s.system.uptime_ms;
  doc["system"]["wifi_ssid"] = coffeeWifiCurrentSsid();
  doc["system"]["wifi_credential_source"] = coffeeWifiCredentialSourceLabel();
  doc["system"]["wifi_stored_credentials"] = coffeeWifiHasStoredCredentials();
  doc["system"]["wifi_stored_credentials_active"] = coffeeWifiStoredCredentialsAreActive();
  doc["system"]["wifi_stored_ssid"] = coffeeWifiStoredSsid();
  doc["system"]["wifi_stored_password"] = coffeeWifiStoredPasswordAvailable();
  doc["system"]["wifi_setup_ap_active"] = coffeeWifiSetupApActive();
  doc["system"]["wifi_setup_ap_ssid"] = coffeeWifiSetupApSsid();
  doc["system"]["wifi_setup_ap_ip"] = coffeeWifiSetupApIp();
  doc["system"]["web_wizard_active"] = s.system.web_wizard_active;
  doc["system"]["autodetect_paused"] = s.system.autodetect_paused;

  String out;
  serializeJson(doc, out);
  return out;
}

void coffeeWebBroadcastState(const AppState& state)
{
  if (!coffeeWebHasClients()) {
    return;
  }
  ws.textAll(buildStateJson(state));
}

bool coffeeWebHasClients()
{
  return ws.count() > 0;
}
