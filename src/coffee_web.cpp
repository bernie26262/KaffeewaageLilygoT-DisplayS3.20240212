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
    .maintenance { display: none; border-left: 6px solid #16a34a; }
    .maintenance-alert { order: -1; }
    .maintenance.show { display: block; }
    .maintenance.ok { border-left-color: #16a34a; background: #f0fdf4; }
    .maintenance.warn { border-left-color: #dc2626; background: #fef2f2; }
    .maintenance-title { font-size: 1.05rem; font-weight: 750; margin-bottom: 6px; }
    .maintenance.ok .maintenance-title { color: #166534; }
    .maintenance.warn .maintenance-title { color: #b91c1c; }
    .maintenance-list { margin: 0; padding-left: 18px; }
    .maintenance-item { margin: 4px 0; }
    .maintenance-item.ok { color: #166534; }
    .maintenance-item.due { color: #b91c1c; font-weight: 750; }
    .autodetect-row { display: flex; justify-content: space-between; align-items: center; gap: 12px; margin-bottom: 8px; padding-bottom: 8px; border-bottom: 1px solid #f3f4f6; }
    .autodetect-toggle { width: auto; min-width: 0; display: inline-flex; align-items: center; gap: 8px; padding: 7px 11px; border-radius: 999px; font-size: .9rem; background: #e5e7eb; color: #374151; }
    .autodetect-toggle.on { background: #dcfce7; color: #166534; }
    .autodetect-toggle.off { background: #e5e7eb; color: #6b7280; }
    .autodetect-toggle.paused { background: #fef3c7; color: #92400e; }
    .autodetect-led { width: 11px; height: 11px; border-radius: 50%; background: #9ca3af; box-shadow: inset 0 0 0 1px rgba(0,0,0,.12); }
    .autodetect-led.on { background: #22c55e; box-shadow: 0 0 0 3px rgba(34,197,94,.18), 0 0 10px rgba(34,197,94,.65); }
    .weight { font-size: 4rem; line-height: 1; font-weight: 750; letter-spacing: -0.06em; margin: 12px 0 4px; text-align: right; font-variant-numeric: tabular-nums; }
    .weight .unit { font-size: 2rem; letter-spacing: 0; margin-left: 6px; }
    .weight-footer { display: flex; justify-content: space-between; align-items: end; gap: 12px; flex-wrap: wrap; }
    .weight-info { min-width: 0; }
    .grid { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 12px; }
    .label { color: #6b7280; font-size: .86rem; }
    .value { font-size: 1.25rem; font-weight: 650; margin-top: 4px; }
    button { width: 100%; border: 0; border-radius: 16px; padding: 16px; font-size: 1.15rem; font-weight: 750; background: #2563eb; color: white; cursor: pointer; }
    button.compact { width: auto; min-width: 150px; padding: 13px 18px; }
    button.secondary { background: #4b5563; }
    .button-row { display: flex; gap: 10px; align-items: center; flex-wrap: wrap; }
    button:disabled { background: #9ca3af; cursor: not-allowed; }
    select, input { border: 1px solid #d1d5db; border-radius: 10px; padding: 6px 9px; font: inherit; font-size: .9rem; background: white; color: #1f2937; }
    input[type=number] { width: 86px; font-variant-numeric: tabular-nums; }
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
    .settings-actions { display: grid; gap: 10px; margin-top: 12px; }
    .gefaess-list { display: grid; gap: 8px; margin-top: 10px; }
    .gefaess-row { display: flex; justify-content: space-between; align-items: center; gap: 12px; padding: 9px 0; border-bottom: 1px solid #f3f4f6; }
    .gefaess-row:last-child { border-bottom: 0; }
    .gefaess-weight { font-weight: 700; }
    .gefaess-missing { color: #6b7280; }
    .page { display: grid; gap: 14px; }
    .page.hidden { display: none; }
    .nav-button { width: auto; min-width: 150px; padding: 10px 14px; font-size: .95rem; }
    .modal-backdrop { position: fixed; inset: 0; display: none; place-items: center; padding: 18px; background: rgba(17, 24, 39, .55); z-index: 1000; }
    .modal-backdrop.show { display: grid; }
    .modal { width: min(420px, 100%); background: #fff; border-radius: 20px; padding: 20px; box-shadow: 0 24px 70px rgba(0,0,0,.28); }
    .modal-title { font-size: 1.25rem; font-weight: 800; margin-bottom: 8px; }
    .modal-text { color: #4b5563; margin-bottom: 16px; }
    .modal-actions { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }
    .wizard-field { display: grid; gap: 6px; margin: 12px 0; }
    .wizard-field input, .wizard-field select { width: 100%; box-sizing: border-box; padding: 11px 12px; font-size: 1rem; }
    .wizard-note { background: #f9fafb; border: 1px solid #e5e7eb; border-radius: 12px; padding: 10px 12px; margin: 12px 0; font-size: .92rem; color: #374151; }
    .wizard-note.warn { background: #fffbeb; border-color: #fde68a; color: #92400e; }
    .danger { background: #dc2626; }
    @media (max-width: 640px) { .grid, .stats-grid { grid-template-columns: 1fr; } .weight { font-size: 3.4rem; } button.compact, .button-row, .nav-button { width: 100%; } .modal-actions { grid-template-columns: 1fr; } }
  </style>
</head>
<body>
<main>
  <section class="card top">
    <h1>Single-Dose-Kaffeewaage</h1>
    <span id="ws" class="pill">WS getrennt</span>
  </section>

  <div id="dashboardPage" class="page">
  <section class="card">
    <div class="autodetect-row small">
      <span>Autodetect</span>
      <button id="autodetectToggle" class="autodetect-toggle off" type="button" aria-pressed="false">
        <span id="autodetectLed" class="autodetect-led"></span>
        <span id="autodetectStatus">---</span>
      </button>
    </div>
    <div class="label">Gewicht</div>
    <div class="weight"><span id="actual">--.-</span><span class="unit">g</span></div>
    <div class="weight-footer">
      <div class="weight-info small">
        Status: <b><span id="status">---</span></b><br>
        Siebträger:
        <select id="siebtraegerSelect" aria-label="Siebträger auswählen">
          <option value="0">Bodenloser ST</option>
          <option value="1">1er-Siebträger</option>
          <option value="2">2er-Siebträger</option>
          <option value="3">Custom-ST</option>
        </select><br>
        Sollgewicht:
        <input id="targetWeight" type="text" inputmode="decimal" aria-label="Sollgewicht in Gramm">
        g
        <button id="targetSave" class="compact secondary" style="min-width: 110px; padding: 8px 12px; font-size: .9rem; margin-left: 6px;">Speichern</button>
      </div>
      <div class="button-row">
        <button id="tare" class="compact secondary">Tara</button>
        <button id="save" class="compact" disabled>Save Dose</button>
      </div>
    </div>
  </section>

  <section id="maintenanceCard" class="card maintenance maintenance-alert ok">
    <div class="maintenance-title" id="maintenanceTitle">Wartung: ok</div>
    <ul class="maintenance-list" id="maintenanceList"></ul>
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
      <div>Wartung zurücksetzen</div>
      <div>Waage kalibrieren</div>
      <div>Gefäße einmessen</div>
    </div>
    <div class="button-row" style="margin-top: 12px;">
      <button id="openSettings" class="nav-button secondary">Einstellungen öffnen</button>
    </div>
  </section>
  </div>

  <div id="settingsPage" class="page hidden">
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

  <div class="button-row">
    <button id="backDashboard" class="nav-button">Zurück zum Dashboard</button>
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

  <section class="card">
    <div class="label">Log</div>
    <div class="log mono" id="log"></div>
  </section>
</main>

<script>
let ws;
let lastState = null;
let maintenanceDueAtMs = { machine: 0, grinder: 0, filter: 0 };
let stopwatchBaseClientMs = 0;
let stopwatchBaseMs = 0;
let stopwatchRunning = false;
let targetWeightDirty = false;
let pendingConfirm = null;
let wizard = { type: null, step: 0 };
let wizardEndSent = true;
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

function showSettings(show) {
  el('dashboardPage').classList.toggle('hidden', show);
  el('settingsPage').classList.toggle('hidden', !show);
}

function openConfirmOverlay(title, text, cmd, logText) {
  pendingConfirm = { cmd, logText };
  el('confirmTitle').textContent = title;
  el('confirmText').textContent = text;
  el('confirmOverlay').classList.add('show');
  el('confirmCancel').focus();
}

function closeConfirmOverlay() {
  el('confirmOverlay').classList.remove('show');
  pendingConfirm = null;
}

function confirmPendingAction() {
  if (!pendingConfirm) return;
  const action = pendingConfirm;
  closeConfirmOverlay();
  sendCommand(action.cmd, action.logText);
}

function sendCommand(cmd, logText) {
  if (!ws || ws.readyState !== WebSocket.OPEN) {
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

function finishWizardOverlay() {
  if (!wizardEndSent) {
    sendCommand('web_wizard_end', 'Assistent beendet, Autodetect wiederherstellen …');
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
      '<div class="wizard-note">Die aktuelle Anzeige sollte danach nahe 0,0 g stehen.</div>',
      [
        wizardButton('Abbrechen', 'secondary', closeWizardOverlay),
        wizardButton('Tarieren', '', () => sendCommandAndThen('web_wizard_tare', 'Kalibrierung: Tara gesendet …', 1))
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
          sendCommandAndThen(`scale_calibration_set_weight_${value.toFixed(1)}`, `Kalibriergewicht gesendet: ${value.toFixed(1)} g`, 2);
        })
      ]
    );
    el('calibrationWeightInput').focus();
    return;
  }

  if (wizard.step === 2) {
    const current = Number(lastState?.calibration?.set_weight_g || 0).toFixed(1);
    setWizardContent(
      'Waage kalibrieren: Kalibrieren',
      `Bitte warten, bis das Gewicht ruhig steht. Eingestelltes Kalibriergewicht: ${current} g.`,
      '<div class="wizard-note">Danach wird der Kalibrierfaktor berechnet und gespeichert.</div>',
      [
        wizardButton('Zurück', 'secondary', () => { wizard.step = 1; renderWizard(); }),
        wizardButton('Kalibrieren', '', () => sendCommandAndThen('scale_calibration_apply', 'Kalibrierung gesendet …', 3))
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
  const names = ['Gefäß 1', 'Gefäß 2', 'Gefäß 3', 'Gefäß 4'];
  return names[index] || `Gefäß ${index + 1}`;
}

function renderMeasureGefaessWizard() {
  if (wizard.step === 0) {
    const current = Number(lastState?.selection?.gefaess ?? 0);
    const options = [0, 1, 2, 3].map(i => `<option value="${i}" ${i === current ? 'selected' : ''}>${gefaessName(i)}</option>`).join('');
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
      '<div class="wizard-note">Danach das ausgewählte Gefäß auflegen.</div>',
      [
        wizardButton('Zurück', 'secondary', () => { wizard.step = 0; renderWizard(); }),
        wizardButton('Tarieren', '', () => sendCommandAndThen('web_wizard_tare', 'Gefäß einmessen: Tara gesendet …', 2))
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
        wizardButton('Gewicht speichern', '', () => sendCommandAndThen('measure_gefaess_save', 'Gefäßgewicht speichern gesendet …', 3))
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

function renderWizard() {
  if (wizard.type === 'calibration') renderCalibrationWizard();
  if (wizard.type === 'gefaess') renderMeasureGefaessWizard();
}

function updateWizardLiveFields() {
  const weightEl = el('gefaessLiveWeight');
  if (weightEl) {
    weightEl.textContent = `${fmtG(lastState?.weight?.actual_g)} g`;
  }
}

function openWizard(type) {
  wizard = { type, step: 0 };
  wizardEndSent = false;
  sendCommand('web_wizard_begin', 'Assistent gestartet, Autodetect pausieren …');
  el('wizardOverlay').classList.add('show');
  renderWizard();
}

function openCalibrationWizard() {
  openWizard('calibration');
}

function openMeasureGefaessWizard() {
  openWizard('gefaess');
}

function fmtDuration(seconds) {
  const total = Math.max(0, Math.floor(Math.abs(Number(seconds) || 0)));
  const days = Math.floor(total / 86400);
  const hours = Math.floor((total % 86400) / 3600);
  const minutes = Math.floor((total % 3600) / 60);
  const secs = total % 60;

  const dayLabel = days === 1 ? 'Tag' : 'Tagen';
  const hourLabel = hours === 1 ? 'Stunde' : 'Stunden';
  const minuteLabel = minutes === 1 ? 'Minute' : 'Minuten';
  const secondLabel = secs === 1 ? 'Sekunde' : 'Sekunden';
  return `${days} ${dayLabel}, ${hours} ${hourLabel}, ${minutes} ${minuteLabel}, ${secs} ${secondLabel}`;
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
  el('stopwatch').textContent = fmtTime(currentStopwatchMs());
  el('swToggle').textContent = stopwatchRunning ? 'Stop' : 'Start';
}

function renderGefaessSettings(s) {
  const list = el('gefaessList');
  if (!list) return;

  const weights = s?.gefaesse?.weights_g || [];
  list.innerHTML = [0, 1, 2, 3].map(index => {
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

function render(s) {
  lastState = s;
  syncMaintenanceTimers(s.maintenance);
  el('actual').textContent = fmtG(s.weight?.actual_g);
  if (!targetWeightDirty && document.activeElement !== el('targetWeight')) {
    el('targetWeight').value = fmtG(s.weight?.set_g);
  }
  el('status').textContent = s.status?.label || String(s.status?.mode ?? '---');
  el('siebtraegerSelect').value = String(s.selection?.siebtraeger ?? 0);
  const autodetectOn = !!s.selection?.autodetect;
  const autodetectPaused = !!s.system?.autodetect_paused;
  const autodetectToggle = el('autodetectToggle');
  const autodetectLed = el('autodetectLed');
  el('autodetectStatus').textContent = autodetectPaused ? 'pausiert' : (autodetectOn ? 'an' : 'aus');
  autodetectToggle.classList.toggle('on', autodetectOn && !autodetectPaused);
  autodetectToggle.classList.toggle('off', !autodetectOn && !autodetectPaused);
  autodetectToggle.classList.toggle('paused', autodetectPaused);
  autodetectToggle.setAttribute('aria-pressed', autodetectOn ? 'true' : 'false');
  autodetectToggle.disabled = autodetectPaused;
  autodetectToggle.title = autodetectPaused ? 'Autodetect ist während des Assistenten pausiert' : (autodetectOn ? 'Autodetect ausschalten' : 'Autodetect einschalten');
  autodetectLed.classList.toggle('on', autodetectOn && !autodetectPaused);
  syncStopwatchTimer(s.stopwatch);
  renderStopwatch();
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
  el('swToggle').disabled = false;
  el('swReset').disabled = false;
  renderMaintenance(s.maintenance);
  renderGefaessSettings(s);
  if (el('wizardOverlay').classList.contains('show')) {
    updateWizardLiveFields();
  }
}

function connect() {
  const proto = location.protocol === 'https:' ? 'wss:' : 'ws:';
  ws = new WebSocket(`${proto}//${location.host}/ws`);
  ws.onopen = () => {
    el('ws').textContent = 'WS verbunden';
    el('ws').classList.add('ok');
    el('swToggle').disabled = false;
    el('swReset').disabled = false;
    addLog('WebSocket verbunden');
  };
  ws.onclose = () => {
    el('ws').textContent = 'WS getrennt';
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

el('save').addEventListener('click', () => {
  if (lastState?.status?.save_ready) {
    sendCommand('save_dose', 'Save gesendet …');
  }
});

el('tare').addEventListener('click', () => {
  sendCommand('tare', 'Tara gesendet …');
});

el('swToggle').addEventListener('click', () => {
  sendCommand('stopwatch_start_stop', stopwatchRunning ? 'Stoppuhr Stop gesendet …' : 'Stoppuhr Start gesendet …');
});

el('swReset').addEventListener('click', () => {
  sendCommand('stopwatch_reset', 'Stoppuhr Reset gesendet …');
});

el('siebtraegerSelect').addEventListener('change', () => {
  const idx = Number(el('siebtraegerSelect').value);
  sendCommand(`select_siebtraeger_${idx}`, `Siebträger-Auswahl gesendet: ${idx}`);
});

function sendTargetWeight() {
  if (!ws || ws.readyState !== WebSocket.OPEN) return;
  const value = Number(el('targetWeight').value.replace(',', '.'));
  if (!Number.isFinite(value) || value <= 0) {
    addLog('Ungültiges Sollgewicht');
    return;
  }
  targetWeightDirty = false;
  el('targetWeight').value = value.toFixed(1);
  sendCommand(`set_selected_siebtraeger_weight_${value.toFixed(1)}`, `Sollgewicht gesendet: ${value.toFixed(1)} g`);
}

el('targetSave').addEventListener('click', sendTargetWeight);
el('openSettings').addEventListener('click', () => showSettings(true));
el('backDashboard').addEventListener('click', () => showSettings(false));
el('openCalibrate').addEventListener('click', openCalibrationWizard);
el('openMeasureGefaess').addEventListener('click', openMeasureGefaessWizard);
el('autodetectToggle').addEventListener('click', () => {
  const autodetectOn = !!lastState?.selection?.autodetect;
  sendCommand(autodetectOn ? 'autodetect_off' : 'autodetect_on', autodetectOn ? 'Autodetect aus gesendet …' : 'Autodetect an gesendet …');
});
el('resetMachine').addEventListener('click', () => openConfirmOverlay(
  'Kaffeemaschinenreinigung reset?',
  'Dadurch werden Zeitpunkt und Zähler seit der letzten Kaffeemaschinenreinigung zurückgesetzt.',
  'maintenance_reset_machine',
  'Reset Kaffeemaschine gesendet …'
));
el('resetGrinder').addEventListener('click', () => openConfirmOverlay(
  'Mühlenreinigung reset?',
  'Dadurch werden Zeitpunkt und Zähler seit der letzten Mühlenreinigung zurückgesetzt.',
  'maintenance_reset_grinder',
  'Reset Mühle gesendet …'
));
el('resetFilter').addEventListener('click', () => openConfirmOverlay(
  'Filterwechsel reset?',
  'Dadurch werden Zeitpunkt und Zähler seit dem letzten Filterwechsel zurückgesetzt.',
  'maintenance_reset_filter',
  'Reset Filter gesendet …'
));
el('gefaessList').addEventListener('click', e => {
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
});
el('confirmCancel').addEventListener('click', closeConfirmOverlay);
el('confirmOk').addEventListener('click', confirmPendingAction);
el('confirmOverlay').addEventListener('click', e => {
  if (e.target === el('confirmOverlay')) closeConfirmOverlay();
});
el('wizardOverlay').addEventListener('click', e => {
  if (e.target === el('wizardOverlay')) closeWizardOverlay();
});
document.addEventListener('keydown', e => {
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
});
el('targetWeight').addEventListener('input', () => { targetWeightDirty = true; });
el('targetWeight').addEventListener('keydown', e => {
  if (e.key === 'Enter') {
    e.preventDefault();
    sendTargetWeight();
  }
});

setInterval(function maintenanceClientTick() {
  if (!lastState) return;
  renderMaintenance(lastState.maintenance);
}, 1000);

setInterval(function stopwatchClientTick() {
  renderStopwatch();
}, 100);

connect();

window.addEventListener('beforeunload', () => {
  if (ws && ws.readyState === WebSocket.OPEN && !wizardEndSent) {
    ws.send(JSON.stringify({ cmd: 'web_wizard_end' }));
  }
});
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

  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(204);
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
  StaticJsonDocument<2048> doc;

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
  doc["system"]["uptime_ms"] = s.system.uptime_ms;
  doc["system"]["web_wizard_active"] = s.system.web_wizard_active;
  doc["system"]["autodetect_paused"] = s.system.autodetect_paused;

  String out;
  serializeJson(doc, out);
  return out;
}

void coffeeWebBroadcastState(const AppState& state)
{
  ws.textAll(buildStateJson(state));
}
