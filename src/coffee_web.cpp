#include "coffee_web.h"
#include <ArduinoJson.h>

static AsyncWebSocket ws("/ws");
static CoffeeWebCommandHandler commandHandler = nullptr;

static const char INDEX_HTML[] PROGMEM = R"rawliteral(<!doctype html>
<html lang="de">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Single-Dose-Kaffeewaage</title>
  <style>
    :root { font-family: system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif; color: #1f2937; background: #f3f4f6; }
    body { margin: 0; padding: 18px; }
    main { max-width: 760px; margin: 0 auto; display: grid; gap: 14px; }
    .card { background: #fff; border-radius: 18px; padding: 18px; box-shadow: 0 8px 24px rgba(0,0,0,.08); }
    .top { display: flex; justify-content: space-between; align-items: center; gap: 12px; }
    h1 { margin: 0; font-size: 1.35rem; }
    .pill { padding: 6px 10px; border-radius: 999px; background: #e5e7eb; font-size: .9rem; }
    .pill.ok { background: #dcfce7; }
    .weight { font-size: 4rem; line-height: 1; font-weight: 750; letter-spacing: -0.06em; margin: 12px 0 4px; text-align: right; font-variant-numeric: tabular-nums; }
    .weight .unit { font-size: 2rem; letter-spacing: 0; margin-left: 6px; }
    .weight-footer { display: flex; justify-content: space-between; align-items: end; gap: 12px; flex-wrap: wrap; }
    .weight-info { min-width: 0; }
    .grid { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 12px; }
    .label { color: #6b7280; font-size: .86rem; }
    .value { font-size: 1.25rem; font-weight: 650; margin-top: 4px; }
    button { width: 100%; border: 0; border-radius: 16px; padding: 16px; font-size: 1.15rem; font-weight: 750; background: #2563eb; color: white; cursor: pointer; }
    button.compact { width: auto; min-width: 160px; padding: 13px 18px; }
    button:disabled { background: #9ca3af; cursor: not-allowed; }
    select { border: 1px solid #d1d5db; border-radius: 10px; padding: 6px 9px; font: inherit; font-size: .9rem; background: white; color: #1f2937; }
    .small { font-size: .9rem; color: #6b7280; }
    .mono { font-family: ui-monospace, SFMono-Regular, Consolas, monospace; }
    .log { margin-top: 12px; padding: 10px 12px; border-radius: 12px; background: #f9fafb; border: 1px solid #e5e7eb; font-size: .9rem; color: #374151; min-height: 1.2em; }
    .log:empty { display: none; }
    .stats-grid { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 18px; }
    .stats-title { font-size: 1.05rem; font-weight: 750; margin-bottom: 8px; }
    .stat-row { display: flex; justify-content: space-between; gap: 12px; padding: 5px 0; border-bottom: 1px solid #f3f4f6; }
    .stat-row:last-child { border-bottom: 0; }
    .stat-row span:first-child { color: #6b7280; }
    .stat-row span:last-child { font-weight: 650; text-align: right; font-variant-numeric: tabular-nums; }
    .ip-row { margin-top: 14px; padding-top: 12px; border-top: 1px solid #e5e7eb; }
    .settings-list { display: grid; gap: 8px; margin-top: 8px; }
    @media (max-width: 640px) { .grid, .stats-grid { grid-template-columns: 1fr; } .weight { font-size: 3.4rem; } button.compact { width: 100%; } }
  </style>
</head>
<body>
<main>
  <section class="card top">
    <h1>Single-Dose-Kaffeewaage</h1>
    <span id="ws" class="pill">WS getrennt</span>
  </section>

  <section class="card">
    <div class="label">Gewicht</div>
    <div class="weight"><span id="actual">--.-</span><span class="unit">g</span></div>
    <div class="weight-footer">
      <div class="weight-info small">
        Soll: <b><span id="set">--.-</span> g</b> · Status: <b><span id="status">---</span></b><br>
        Siebträger:
        <select id="siebtraegerSelect" aria-label="Siebträger auswählen">
          <option value="0">Bodenloser ST</option>
          <option value="1">1er-Siebträger</option>
          <option value="2">2er-Siebträger</option>
          <option value="3">Custom-ST</option>
        </select>
      </div>
      <button id="save" class="compact" disabled>Save Dose</button>
    </div>
  </section>

  <section class="card">
    <div class="label">Stoppuhr</div>
    <div class="value" id="stopwatch">00:00.0</div>
    <div class="grid" style="margin-top: 12px;">
      <button id="swToggle" disabled>Start / Stop</button>
      <button id="swReset" disabled>Reset</button>
    </div>
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
      <div>IP-Adresse: <b class="mono" id="ip">---</b></div>
    </div>
  </section>

  <section class="card">
    <div class="stats-title">Einstellungen</div>
    <div class="settings-list small">
      <div>Reinigung Kaffeemaschine zurücksetzen</div>
      <div>Reinigung Mühle zurücksetzen</div>
      <div>Filterwechsel zurücksetzen</div>
      <div>Waage kalibrieren</div>
      <div>Gefäße einmessen</div>
    </div>
  </section>

  <section class="card">
    <div class="label">Log</div>
    <div class="log mono" id="log"></div>
  </section>
</main>

<script>
let ws;
let lastState = null;
const el = id => document.getElementById(id);
const fmtG = v => {
  let n = Number(v || 0);
  if (n > -0.05 && n < 0.05) n = 0;
  return n.toFixed(1);
};
const fmtKg = g => (Number(g || 0) / 1000).toFixed(2);
const fmtWholeG = g => String(Math.round(Number(g || 0)));
const fmtTime = ms => {
  ms = Number(ms || 0);
  const m = Math.floor(ms / 60000);
  const s = Math.floor((ms % 60000) / 1000);
  const d = Math.floor((ms % 1000) / 100);
  return `${String(m).padStart(2, '0')}:${String(s).padStart(2, '0')}.${d}`;
};
const fmtDateTime = (epoch, valid) => {
  if (!valid || !epoch) return 'nicht synchronisiert';
  return new Date(Number(epoch) * 1000).toLocaleString('de-DE', {
    day: '2-digit', month: '2-digit', year: 'numeric',
    hour: '2-digit', minute: '2-digit', second: '2-digit'
  });
};
const fmtUptime = ms => {
  let sec = Math.floor(Number(ms || 0) / 1000);
  const d = Math.floor(sec / 86400); sec %= 86400;
  const h = Math.floor(sec / 3600); sec %= 3600;
  const m = Math.floor(sec / 60);
  const s = sec % 60;
  if (d > 0) return `${d} d ${h} h ${m} min`;
  if (h > 0) return `${h} h ${m} min`;
  return `${m} min ${s} s`;
};

function addLog(msg) {
  const now = new Date().toLocaleTimeString();
  el('log').textContent = `${now}  ${msg}`;
}

function render(s) {
  lastState = s;
  el('actual').textContent = fmtG(s.weight?.actual_g);
  el('set').textContent = fmtG(s.weight?.set_g);
  el('status').textContent = s.status?.label || String(s.status?.mode ?? '---');
  el('siebtraegerSelect').value = String(s.selection?.siebtraeger ?? 0);
  el('stopwatch').textContent = fmtTime(s.stopwatch?.ms);
  el('shotsTotal').textContent = s.stats?.shots?.total ?? 0;
  el('shotsMachine').textContent = s.stats?.shots?.since_machine_clean ?? 0;
  el('shotsGrinder').textContent = s.stats?.shots?.since_grinder_clean ?? 0;
  el('shotsFilter').textContent = s.stats?.shots?.since_filter_change ?? 0;
  el('groundTotalKg').textContent = fmtKg(s.stats?.ground?.total_g);
  el('groundMachineG').textContent = fmtWholeG(s.stats?.ground?.since_machine_clean_g);
  el('groundGrinderG').textContent = fmtWholeG(s.stats?.ground?.since_grinder_clean_g);
  el('groundFilterG').textContent = fmtWholeG(s.stats?.ground?.since_filter_change_g);
  el('datetime').textContent = fmtDateTime(s.time?.epoch, s.time?.valid);
  el('uptime').textContent = fmtUptime(s.system?.uptime_ms);
  el('ip').textContent = s.system?.ip || location.hostname;
  el('save').disabled = !s.status?.save_ready;
}

function connect() {
  const proto = location.protocol === 'https:' ? 'wss:' : 'ws:';
  ws = new WebSocket(`${proto}//${location.host}/ws`);
  ws.onopen = () => { el('ws').textContent = 'WS verbunden'; el('ws').classList.add('ok'); addLog('WebSocket verbunden'); };
  ws.onclose = () => { el('ws').textContent = 'WS getrennt'; el('ws').classList.remove('ok'); addLog('WebSocket getrennt, reconnect läuft …'); setTimeout(connect, 1500); };
  ws.onerror = () => { addLog('WebSocket-Fehler'); };
  ws.onmessage = e => {
    const data = JSON.parse(e.data);
    if (data.type === 'state') render(data);
    if (data.type === 'ack') addLog(`OK: ${data.cmd}`);
    if (data.type === 'error') addLog(`Fehler: ${data.cmd || ''} ${data.message}`);
  };
}

el('save').addEventListener('click', () => {
  if (ws && ws.readyState === WebSocket.OPEN && lastState?.status?.save_ready) {
    ws.send(JSON.stringify({ cmd: 'save_dose' }));
    addLog('Save gesendet …');
  }
});

el('siebtraegerSelect').addEventListener('change', () => {
  if (ws && ws.readyState === WebSocket.OPEN) {
    const idx = Number(el('siebtraegerSelect').value);
    ws.send(JSON.stringify({ cmd: `select_siebtraeger_${idx}` }));
    addLog(`Siebträger-Auswahl gesendet: ${idx}`);
  }
});

connect();
</script>
</body>
</html>)rawliteral";

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

void coffeeWebHandleRoot(AsyncWebServerRequest* request)
{
  if (!request) {
    return;
  }
  request->send_P(200, "text/html", INDEX_HTML);
}


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

static String buildStateJson(const AppState& s)
{
  StaticJsonDocument<1024> doc;

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

  doc["stats"]["ground"]["total_g"] = s.stats.ground.total_g;
  doc["stats"]["ground"]["since_grinder_clean_g"] = s.stats.ground.since_grinder_clean_g;
  doc["stats"]["ground"]["since_machine_clean_g"] = s.stats.ground.since_machine_clean_g;
  doc["stats"]["ground"]["since_filter_change_g"] = s.stats.ground.since_filter_change_g;

  doc["stats"]["shots"]["total"] = s.stats.shots.total;
  doc["stats"]["shots"]["since_grinder_clean"] = s.stats.shots.since_grinder_clean;
  doc["stats"]["shots"]["since_machine_clean"] = s.stats.shots.since_machine_clean;
  doc["stats"]["shots"]["since_filter_change"] = s.stats.shots.since_filter_change;

  doc["time"]["valid"] = s.time.valid;
  doc["time"]["epoch"] = s.time.epoch;

  doc["system"]["wifi"] = s.system.wifi_connected;
  doc["system"]["ip"] = s.system.ip;
  doc["system"]["uptime_ms"] = s.system.uptime_ms;

  String out;
  serializeJson(doc, out);
  return out;
}

void coffeeWebBroadcastState(const AppState& state)
{
  ws.textAll(buildStateJson(state));
}