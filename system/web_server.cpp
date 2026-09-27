
#include "web_server.h"
#include <WebServer.h>
#include <Preferences.h>
#include <Update.h>
#include "globals.h"

static WebServer server(80);
static TaskHandle_t webServerTaskHandle = NULL;
static volatile bool serverRunning = false;
static volatile bool rebootRequested = false;
// BUG FIX: removed dangling "extern Preferences prefs" — the main sketch closes
// prefs before the web server starts, so that handle is invalid here. Each
// handler opens its own local Preferences instance instead.

// ─────────────────────────────────────────────────────────────────────────────
// Beautiful premium dark dashboard HTML
// ─────────────────────────────────────────────────────────────────────────────
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Macro Keyboard — Dashboard</title>
  <style>
    /* FIX 31: Removed Google Fonts CDN import — it fails silently in SoftAP mode
       (no internet connection), breaking typography. System font stack provides
       excellent native UI fonts on all platforms without any network request. */
    *{margin:0;padding:0;box-sizing:border-box}
    :root{
      --bg:#07070d;--surface:#111119;--card:#181824;--border:#252535;
      --accent:#8b7ff5;--accent2:#5b8af5;--danger:#e05c6f;--success:#4cd97b;
      --warn:#f0a84a;--text:#eeeef8;--muted:#5e5e7a;--muted2:#888;
    }
    html{scroll-behavior:smooth}
    body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,Helvetica,Arial,sans-serif;background:var(--bg);color:var(--text);min-height:100vh}
    .topbar{
      background:linear-gradient(135deg,rgba(26,26,46,0.97),rgba(16,16,36,0.99));
      border-bottom:1px solid var(--border);padding:16px 20px;
      display:flex;align-items:center;justify-content:space-between;
      position:sticky;top:0;z-index:100;
      backdrop-filter:blur(14px);-webkit-backdrop-filter:blur(14px);
      box-shadow:0 4px 32px rgba(0,0,0,0.6)
    }
    .logo{display:flex;align-items:center;gap:12px}
    .logo-icon{
      width:40px;height:40px;
      background:linear-gradient(135deg,var(--accent),var(--accent2));
      border-radius:12px;display:flex;align-items:center;justify-content:center;
      font-size:20px;box-shadow:0 4px 16px rgba(139,127,245,0.35)
    }
    .logo-text h1{
      font-size:1.1rem;font-weight:700;
      background:linear-gradient(135deg,#b8aaff,#7ab0ff);
      -webkit-background-clip:text;-webkit-text-fill-color:transparent
    }
    .logo-text p{font-size:0.68rem;color:var(--muted);margin-top:1px}
    .status-pill{
      padding:7px 14px;border-radius:20px;font-size:0.73rem;font-weight:600;
      display:flex;align-items:center;gap:7px;transition:all 0.3s
    }
    .status-pill.connected{background:rgba(76,217,123,0.12);border:1px solid rgba(76,217,123,0.35);color:var(--success)}
    .status-pill.ap{background:rgba(240,168,74,0.12);border:1px solid rgba(240,168,74,0.35);color:var(--warn)}
    .status-pill.offline{background:rgba(224,92,111,0.12);border:1px solid rgba(224,92,111,0.3);color:var(--danger)}
    .status-dot{width:7px;height:7px;border-radius:50%;background:currentColor;animation:pulse 2s infinite}
    @keyframes pulse{0%,100%{opacity:1;transform:scale(1)}50%{opacity:0.45;transform:scale(0.82)}}
    .container{max-width:620px;margin:0 auto;padding:22px 14px 40px}
    .section{margin-bottom:20px}
    .card{
      background:var(--card);border:1px solid var(--border);border-radius:18px;
      overflow:hidden;box-shadow:0 4px 32px rgba(0,0,0,0.35)
    }
    .card-header{
      padding:15px 18px;border-bottom:1px solid var(--border);
      display:flex;align-items:center;gap:10px;background:rgba(255,255,255,0.018)
    }
    .card-header .icon{width:32px;height:32px;border-radius:9px;display:flex;align-items:center;justify-content:center;font-size:15px}
    .card-header h2{font-size:0.92rem;font-weight:600;flex:1}
    .card-body{padding:18px}
    .net-list{display:flex;flex-direction:column;gap:7px;margin-bottom:14px}
    .net-item{
      display:flex;align-items:center;gap:10px;background:var(--surface);
      border:1px solid var(--border);border-radius:11px;padding:11px 13px;transition:all 0.2s
    }
    .net-item.is-connected{border-color:rgba(76,217,123,0.45);background:rgba(76,217,123,0.055)}
    .net-item:hover{border-color:var(--accent);box-shadow:0 0 0 1px rgba(139,127,245,0.18) inset}
    .net-idx{font-size:0.68rem;color:var(--muted);background:var(--border);padding:2px 8px;border-radius:10px;font-weight:700;min-width:28px;text-align:center}
    .net-name{flex:1;font-size:0.88rem;font-weight:500;word-break:break-all}
    .connected-badge{
      font-size:0.66rem;font-weight:700;color:var(--success);
      background:rgba(76,217,123,0.15);border:1px solid rgba(76,217,123,0.3);
      padding:2px 7px;border-radius:8px;white-space:nowrap
    }
    .net-actions{display:flex;gap:6px;flex-shrink:0}
    .btn-up{
      background:rgba(91,138,245,0.12);border:1px solid rgba(91,138,245,0.25);
      color:var(--accent2);border-radius:7px;padding:5px 9px;
      font-size:0.75rem;font-weight:600;cursor:pointer;transition:all 0.2s;line-height:1
    }
    .btn-up:hover:not(:disabled){background:var(--accent2);color:#fff;border-color:var(--accent2)}
    .btn-up:disabled,.btn-del:disabled{opacity:0.38;cursor:not-allowed}
    .btn-del{
      background:rgba(224,92,111,0.12);border:1px solid rgba(224,92,111,0.28);
      color:var(--danger);border-radius:7px;padding:5px 11px;
      font-size:0.75rem;font-weight:600;cursor:pointer;transition:all 0.2s;white-space:nowrap
    }
    .btn-del:hover:not(:disabled){background:var(--danger);color:#fff;border-color:var(--danger)}
    .empty-state{text-align:center;padding:22px;color:var(--muted);font-size:0.84rem}
    .divider{height:1px;background:var(--border);margin:14px 0}
    .form-row{display:flex;flex-direction:column;gap:10px}
    .input-group{display:flex;flex-direction:column;gap:6px}
    .input-group label{font-size:0.73rem;font-weight:600;color:var(--muted2);text-transform:uppercase;letter-spacing:0.06em}
    .input-wrap{position:relative;display:flex;align-items:center}
    input[type=text],input[type=password]{
      width:100%;padding:11px 14px;background:var(--surface);border:1px solid var(--border);
      border-radius:10px;color:var(--text);font-family:inherit;font-size:0.88rem;outline:none;transition:all 0.2s
    }
    input[type=password]{padding-right:44px}
    input:focus{border-color:var(--accent);box-shadow:0 0 0 3px rgba(139,127,245,0.13)}
    .eye-btn{
      position:absolute;right:12px;background:none;border:none;color:var(--muted);
      cursor:pointer;padding:4px;font-size:16px;line-height:1;transition:color 0.2s
    }
    .eye-btn:hover{color:var(--text)}
    .btn{
      padding:12px 18px;border-radius:10px;font-family:inherit;font-size:0.88rem;font-weight:600;
      cursor:pointer;border:none;transition:all 0.2s;width:100%;
      display:flex;align-items:center;justify-content:center;gap:8px
    }
    .btn:disabled{opacity:0.5;cursor:not-allowed;transform:none!important}
    .btn-primary{background:linear-gradient(135deg,var(--accent),var(--accent2));color:#fff;box-shadow:0 4px 16px rgba(139,127,245,0.25)}
    .btn-primary:hover:not(:disabled){transform:translateY(-1px);box-shadow:0 6px 22px rgba(139,127,245,0.4)}
    .btn-danger{background:rgba(224,92,111,0.12);border:1px solid rgba(224,92,111,0.35);color:var(--danger)}
    .btn-danger:hover{background:var(--danger);color:#fff;border-color:var(--danger)}
    .btn-ota{background:linear-gradient(135deg,#7c3aed,#a855f7);color:#fff;box-shadow:0 4px 16px rgba(168,85,247,0.25)}
    .btn-ota:hover{transform:translateY(-1px);box-shadow:0 6px 22px rgba(168,85,247,0.4)}
    .toggle-row{display:flex;align-items:center;justify-content:space-between;padding:6px 0}
    .toggle-row .label-text{font-size:0.88rem;font-weight:500}
    .toggle{position:relative;display:inline-block;width:50px;height:27px}
    .toggle input{opacity:0;width:0;height:0}
    .slider{position:absolute;inset:0;background:var(--border);border-radius:27px;cursor:pointer;transition:0.3s}
    .slider:before{content:'';position:absolute;width:21px;height:21px;bottom:3px;left:3px;background:#fff;border-radius:50%;transition:0.3s}
    input:checked+.slider{background:linear-gradient(135deg,var(--accent),var(--accent2))}
    input:checked+.slider:before{transform:translateX(23px)}
    select{
      width:100%;padding:11px 14px;background:var(--surface);border:1px solid var(--border);
      border-radius:10px;color:var(--text);font-family:inherit;font-size:0.88rem;outline:none;cursor:pointer;transition:all 0.2s
    }
    select:focus{border-color:var(--accent)}
    .info-grid{display:grid;grid-template-columns:1fr 1fr;gap:8px}
    .info-item{background:var(--surface);border:1px solid var(--border);border-radius:10px;padding:11px 12px}
    .info-item .label{font-size:0.67rem;color:var(--muted);text-transform:uppercase;letter-spacing:0.06em;margin-bottom:5px}
    .info-item .value{font-size:0.88rem;font-weight:600;word-break:break-all}
    .file-input-wrap{
      border:2px dashed var(--border);border-radius:13px;padding:22px;text-align:center;
      transition:all 0.2s;cursor:pointer;position:relative
    }
    .file-input-wrap:hover{border-color:var(--accent);background:rgba(139,127,245,0.04)}
    .file-input-wrap input[type=file]{position:absolute;inset:0;opacity:0;cursor:pointer;width:100%;height:100%}
    .fi-icon{font-size:28px;margin-bottom:8px}
    .file-input-wrap p{font-size:0.83rem;color:var(--muted)}
    .file-name{margin-top:8px;font-size:0.81rem;color:var(--accent);font-weight:500}
    .progress-bar{height:6px;background:var(--border);border-radius:6px;overflow:hidden;margin-top:14px;display:none}
    .progress-bar-fill{height:100%;background:linear-gradient(90deg,var(--accent),var(--accent2));width:0;transition:width 0.3s;border-radius:6px}
    .toast{
      position:fixed;top:18px;right:16px;padding:11px 16px;border-radius:10px;
      font-size:0.83rem;font-weight:500;color:#fff;z-index:999;
      transform:translateX(calc(100% + 30px));transition:transform 0.35s cubic-bezier(0.34,1.56,0.64,1);
      max-width:270px;backdrop-filter:blur(10px);box-shadow:0 8px 24px rgba(0,0,0,0.4)
    }
    .toast.show{transform:translateX(0)}
    .toast.success{background:rgba(20,60,36,0.96);border:1px solid rgba(76,217,123,0.4)}
    .toast.error{background:rgba(70,15,22,0.96);border:1px solid rgba(224,92,111,0.4)}
    .toast.warn{background:rgba(70,45,8,0.96);border:1px solid rgba(240,168,74,0.4)}
    @media(max-width:480px){.info-grid{grid-template-columns:1fr}.net-actions{flex-wrap:wrap}}
  </style>
</head>
<body>
  <div class="topbar">
    <div class="logo">
      <div class="logo-icon">⌨️</div>
      <div class="logo-text">
        <h1>Macro Keyboard</h1>
        <p>Web Dashboard</p>
      </div>
    </div>
    <div id="statusPill" class="status-pill connected">
      <span class="status-dot"></span>
      <span id="statusText">Loading...</span>
    </div>
  </div>

  <div class="container">

    <!-- Device Info -->
    <div class="section">
      <div class="card">
        <div class="card-header">
          <div class="icon" style="background:rgba(139,127,245,0.18)">📡</div>
          <h2>Device Info</h2>
        </div>
        <div class="card-body">
          <div class="info-grid">
            <div class="info-item"><div class="label">IP Address</div><div class="value" id="infoIP">—</div></div>
            <div class="info-item"><div class="label">WiFi SSID</div><div class="value" id="infoSSID">—</div></div>
            <div class="info-item"><div class="label">Silent Mode</div><div class="value" id="infoSilent">—</div></div>
            <div class="info-item"><div class="label">Display Sleep</div><div class="value" id="infoSleep">—</div></div>
          </div>
        </div>
      </div>
    </div>

    <!-- WiFi Networks -->
    <div class="section">
      <div class="card">
        <div class="card-header">
          <div class="icon" style="background:rgba(91,138,245,0.18)">📶</div>
          <h2>Saved WiFi Networks</h2>
        </div>
        <div class="card-body">
          <div class="net-list" id="networks"><div class="empty-state">Loading...</div></div>
          <div class="divider"></div>
          <div class="form-row">
            <div class="input-group">
              <label>Network Name (SSID)</label>
              <input type="text" id="ssid" placeholder="e.g. HomeNetwork" autocomplete="off">
            </div>
            <div class="input-group">
              <label>Password</label>
              <div class="input-wrap">
                <input type="password" id="pass" placeholder="WiFi password" autocomplete="new-password">
                <button class="eye-btn" type="button" onclick="togglePass()" title="Show/hide password">👁</button>
              </div>
            </div>
            <button class="btn btn-primary" id="addBtn" onclick="addNetwork()">＋ Add Network</button>
          </div>
        </div>
      </div>
    </div>

    <!-- System Settings -->
    <div class="section">
      <div class="card">
        <div class="card-header">
          <div class="icon" style="background:rgba(76,217,123,0.18)">⚙️</div>
          <h2>System Settings</h2>
        </div>
        <div class="card-body">
          <div class="form-row">
            <div class="toggle-row">
              <span class="label-text">Silent Mode (disable buzzer)</span>
              <label class="toggle"><input type="checkbox" id="silentMode"><span class="slider"></span></label>
            </div>
            <div class="divider"></div>
            <div class="input-group">
              <label>Display Auto-Sleep</label>
              <select id="sleepTimeout">
                <option value="60000">1 Minute</option>
                <option value="300000">5 Minutes</option>
                <option value="900000">15 Minutes</option>
                <option value="0">Never</option>
              </select>
            </div>
            <button class="btn btn-primary" onclick="saveSettings()">💾 Save Settings</button>
          </div>
        </div>
      </div>
    </div>

    <!-- Power -->
    <div class="section">
      <div class="card">
        <div class="card-header">
          <div class="icon" style="background:rgba(224,92,111,0.18)">🔌</div>
          <h2>Power</h2>
        </div>
        <div class="card-body">
          <button class="btn btn-danger" onclick="rebootDevice()">⟳ Reboot Keyboard</button>
        </div>
      </div>
    </div>

    <!-- OTA Update -->
    <div class="section">
      <div class="card">
        <div class="card-header">
          <div class="icon" style="background:rgba(168,85,247,0.18)">🔧</div>
          <h2>OTA Firmware Update</h2>
        </div>
        <div class="card-body">
          <div class="file-input-wrap">
            <input type="file" id="fwFile" name="update" accept=".bin" onchange="fileSelected(this)">
            <div class="fi-icon">📦</div>
            <p>Click to select firmware <strong>.bin</strong> file</p>
            <div class="file-name" id="fileName"></div>
          </div>
          <div class="progress-bar" id="progressBar"><div class="progress-bar-fill" id="progressFill"></div></div>
          <div style="margin-top:14px">
            <button type="button" class="btn btn-ota" onclick="uploadFirmware()">⬆️ Upload Firmware</button>
          </div>
        </div>
      </div>
    </div>

  </div>

  <div class="toast" id="toast"></div>

  <script>
    // ── Utilities ───────────────────────────────────────────────────────────
    let _toastTimer = null;
    function showToast(msg, type='success') {
      const t = document.getElementById('toast');
      t.textContent = msg;
      t.className = 'toast ' + type + ' show';
      clearTimeout(_toastTimer);
      _toastTimer = setTimeout(() => { t.className = 'toast ' + type; }, 3500);
    }

    // BUG FIX: handles all sleep values including non-standard ones
    function sleepLabel(ms) {
      const v = parseInt(ms);
      if (v === 0)      return 'Never';
      if (v === 60000)  return '1 min';
      if (v === 300000) return '5 min';
      if (v === 900000) return '15 min';
      if (v < 60000)    return Math.round(v / 1000) + ' sec';
      return Math.round(v / 60000) + ' min';
    }

    function escHtml(s) {
      return String(s)
        .replace(/&/g,'&amp;').replace(/</g,'&lt;')
        .replace(/>/g,'&gt;').replace(/"/g,'&quot;');
    }

    // ── Device Info ─────────────────────────────────────────────────────────
    // BUG FIX: use d.mode ('sta'/'ap') — never compare ssid to literal "AP"
    function fetchInfo() {
      fetch('/api/info').then(r => r.json()).then(d => {
        document.getElementById('infoIP').textContent    = d.ip   || '—';
        document.getElementById('infoSSID').textContent  = d.ssid || (d.mode === 'ap' ? 'AP Mode' : '—');
        document.getElementById('infoSilent').textContent = d.silentMode ? 'ON' : 'OFF';
        document.getElementById('infoSleep').textContent  = sleepLabel(d.sleepTimeout);

        const pill = document.getElementById('statusPill');
        const txt  = document.getElementById('statusText');
        if (d.mode === 'sta') {
          pill.className = 'status-pill connected'; txt.textContent = 'Connected';
        } else {
          pill.className = 'status-pill ap'; txt.textContent = 'AP Mode';
        }
        window._connSSID = (d.mode === 'sta') ? d.ssid : null;
        highlightConnected();
      }).catch(() => {
        document.getElementById('statusPill').className = 'status-pill offline';
        document.getElementById('statusText').textContent = 'Offline';
      });
    }

    // ── WiFi Networks ────────────────────────────────────────────────────────
    window._connSSID = null;

    function highlightConnected() {
      document.querySelectorAll('.net-item').forEach(el => {
        el.classList.toggle('is-connected', el.dataset.ssid === window._connSSID);
        const badge = el.querySelector('.connected-badge');
        if (el.dataset.ssid === window._connSSID) {
          if (!badge) el.querySelector('.net-name').insertAdjacentHTML('afterend','<span class="connected-badge">ACTIVE</span>');
        } else {
          if (badge) badge.remove();
        }
      });
    }

    function fetchNetworks() {
      fetch('/api/wifi').then(r => r.json()).then(data => {
        const el = document.getElementById('networks');
        if (!data.length) {
          el.innerHTML = '<div class="empty-state">No saved networks. Add one below.</div>';
          return;
        }
        el.innerHTML = data.map((n, i) => `
          <div class="net-item" data-ssid="${escHtml(n.ssid)}" data-id="${i}">
            <span class="net-idx">#${i+1}</span>
            <span class="net-name">${escHtml(n.ssid)}</span>
            <div class="net-actions">
              <button class="btn-up" onclick="moveUp(${i})" ${i===0?'disabled':''} title="Increase priority">▲</button>
              <button class="btn-del" onclick="delNetwork(${i})" title="Remove">Remove</button>
            </div>
          </div>`
        ).join('');
        highlightConnected();
      }).catch(() => {
        document.getElementById('networks').innerHTML = '<div class="empty-state">Failed to load networks.</div>';
      });
    }

    function addNetwork() {
      const ssid = document.getElementById('ssid').value.trim();
      const pass = document.getElementById('pass').value;
      if (!ssid) { showToast('Please enter a network name', 'error'); return; }
      const btn = document.getElementById('addBtn');
      btn.disabled = true; btn.textContent = 'Adding...';
      fetch('/api/wifi/add', {
        method: 'POST',
        headers: {'Content-Type': 'application/x-www-form-urlencoded'},
        body: 'ssid=' + encodeURIComponent(ssid) + '&pass=' + encodeURIComponent(pass)
      }).then(r => {
        if (r.ok) {
          document.getElementById('ssid').value = '';
          document.getElementById('pass').value = '';
          fetchNetworks();
          showToast('✓ Network added!');
        } else { r.text().then(t => showToast('Error: ' + t, 'error')); }
      }).catch(() => showToast('Connection error', 'error'))
        .finally(() => { btn.disabled = false; btn.textContent = '＋ Add Network'; });
    }

    function delNetwork(id) {
      if (!confirm('Remove this network?')) return;
      // BUG FIX: disable all del buttons during request to prevent double-click race
      document.querySelectorAll('.btn-del,.btn-up').forEach(b => b.disabled = true);
      fetch('/api/wifi/del', {
        method: 'POST',
        headers: {'Content-Type': 'application/x-www-form-urlencoded'},
        body: 'id=' + id
      }).then(r => {
        if (r.ok) { fetchNetworks(); showToast('Network removed', 'warn'); }
        else showToast('Failed to remove', 'error');
      }).catch(() => showToast('Failed to remove', 'error'))
        .finally(() => document.querySelectorAll('.btn-del,.btn-up').forEach(b => b.disabled = false));
    }

    function moveUp(id) {
      if (id === 0) return;
      document.querySelectorAll('.btn-del,.btn-up').forEach(b => b.disabled = true);
      fetch('/api/wifi/reorder', {
        method: 'POST',
        headers: {'Content-Type': 'application/x-www-form-urlencoded'},
        body: 'id=' + id
      }).then(r => {
        if (r.ok) { fetchNetworks(); showToast('Priority updated'); }
        else showToast('Failed to reorder', 'error');
      }).catch(() => showToast('Connection error', 'error'))
        .finally(() => document.querySelectorAll('.btn-del,.btn-up').forEach(b => b.disabled = false));
    }

    // ── Settings ─────────────────────────────────────────────────────────────
    function fetchSettings() {
      fetch('/api/settings').then(r => r.json()).then(d => {
        document.getElementById('silentMode').checked = (d.silentMode == 1);
        // BUG FIX: only set select value if that option actually exists
        const sel = document.getElementById('sleepTimeout');
        if (sel.querySelector('option[value="' + d.sleepTimeout + '"]')) {
          sel.value = d.sleepTimeout;
        }
        // else: stored value is non-standard — leave dropdown at its default
      }).catch(() => {});
    }

    function saveSettings() {
      const sil = document.getElementById('silentMode').checked ? 1 : 0;
      const slp = document.getElementById('sleepTimeout').value;
      fetch('/api/settings/update', {
        method: 'POST',
        headers: {'Content-Type': 'application/x-www-form-urlencoded'},
        body: 'silentMode=' + sil + '&sleepTimeout=' + slp
      }).then(r => {
        if (r.ok) { showToast('✓ Settings saved!'); fetchInfo(); }
        else showToast('Failed to save', 'error');
      }).catch(() => showToast('Connection error', 'error'));
    }

    // ── Power ────────────────────────────────────────────────────────────────
    function rebootDevice() {
      if (!confirm('Reboot the keyboard now?')) return;
      fetch('/api/reboot', { method: 'POST' })
        .then(() => showToast('Rebooting… reconnect shortly', 'warn'))
        .catch(() => showToast('Reboot command sent', 'warn'));
    }

    // ── OTA ──────────────────────────────────────────────────────────────────
    function togglePass() {
      const p = document.getElementById('pass');
      p.type = (p.type === 'password') ? 'text' : 'password';
    }

    function fileSelected(input) {
      document.getElementById('fileName').textContent = input.files[0] ? input.files[0].name : '';
    }

    function uploadFirmware() {
      const file = document.getElementById('fwFile').files[0];
      if (!file) { showToast('Please select a .bin file first', 'error'); return; }
      if (!file.name.endsWith('.bin')) { showToast('File must be a .bin firmware', 'error'); return; }
      const bar  = document.getElementById('progressBar');
      const fill = document.getElementById('progressFill');
      bar.style.display = 'block'; fill.style.width = '0%';
      const fd  = new FormData();
      fd.append('update', file);
      const xhr = new XMLHttpRequest();
      xhr.upload.onprogress = e => {
        if (e.lengthComputable) fill.style.width = (e.loaded / e.total * 100).toFixed(1) + '%';
      };
      xhr.onload = () => {
        if (xhr.status === 200) { fill.style.width = '100%'; showToast('✓ Upload complete! Rebooting…'); }
        else showToast('OTA Failed: ' + xhr.responseText, 'error');
      };
      xhr.onerror = () => showToast('Upload failed — connection lost', 'error');
      xhr.open('POST', '/update');
      xhr.send(fd);
    }

    // ── Init ─────────────────────────────────────────────────────────────────
    fetchInfo(); fetchNetworks(); fetchSettings();
    setInterval(fetchInfo, 5000);
  </script>
</body>
</html>
)rawliteral";

// ─────────────────────────────────────────────────────────────────────────────
// API helpers — CORS + content-type on every response
// ─────────────────────────────────────────────────────────────────────────────
static void sendJSON(int code, const String& json) {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(code, "application/json", json);
}
static void sendOK(const String& msg = "OK") {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "text/plain", msg);
}
static void sendErr(int code, const String& msg) {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(code, "text/plain", msg);
}

// ─────────────────────────────────────────────────────────────────────────────
// API Handlers
// ─────────────────────────────────────────────────────────────────────────────

static void handleRoot() {
    server.send_P(200, "text/html", index_html);
}

// BUG FIX: added "mode" field so JS uses d.mode instead of comparing ssid to "AP"
static void handleGetInfo() {
    bool isSTA = (WiFi.status() == WL_CONNECTED);
    String ip   = isSTA ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
    String ssid = isSTA ? WiFi.SSID() : String("");
    String mode = isSTA ? String("sta") : String("ap");
    bool   sm   = silentMode;
    unsigned long slp = sleepTimeoutMs;
    String json = "{\"ip\":\"" + ip + "\""
                  ",\"ssid\":\"" + ssid + "\""
                  ",\"mode\":\"" + mode + "\""
                  ",\"silentMode\":" + (sm ? "1" : "0") +
                  ",\"sleepTimeout\":" + String(slp) + "}";
    sendJSON(200, json);
}

static void handleGetWifi() {
    Preferences p;
    p.begin("macro-kb", true);
    int count = p.getInt("wifi_cnt", 0);
    String json = "[";
    for (int i = 0; i < count; i++) {
        String ssid = p.getString(("ssid_" + String(i)).c_str(), "");
        if (i > 0) json += ",";
        json += "{\"id\":" + String(i) + ",\"ssid\":\"" + ssid + "\"}";
    }
    json += "]";
    p.end();
    sendJSON(200, json);
}

// BUG FIX: populate live array first, then flash, then bump count atomically
static void handleAddWifi() {
    if (!server.hasArg("ssid")) { sendErr(400, "Missing ssid"); return; }
    String ssid = server.arg("ssid");
    String pass = server.arg("pass");
    ssid.trim();
    if (ssid.length() == 0) { sendErr(400, "Empty ssid"); return; }
    Preferences p;
    p.begin("macro-kb", false);
    int count = p.getInt("wifi_cnt", 0);
    if (count >= 50) { p.end(); sendErr(400, "Max 50 networks"); return; }
    // 1. Live array first
    strncpy(WIFI_NETS[count].ssid, ssid.c_str(), 32); WIFI_NETS[count].ssid[32] = '\0';
    strncpy(WIFI_NETS[count].pass, pass.c_str(), 64); WIFI_NETS[count].pass[64] = '\0';
    // 2. Flash
    p.putString(("ssid_" + String(count)).c_str(), ssid);
    p.putString(("pass_" + String(count)).c_str(), pass);
    p.putInt("wifi_cnt", count + 1);
    p.end();
    // 3. Bump count last
    WIFI_NET_COUNT = count + 1;
    sendOK();
}

// BUG FIX: bump WIFI_NET_COUNT only after full array shift
static void handleDelWifi() {
    if (!server.hasArg("id")) { sendErr(400, "Missing id"); return; }
    int id = server.arg("id").toInt();
    Preferences p;
    p.begin("macro-kb", false);
    int count = p.getInt("wifi_cnt", 0);
    if (id < 0 || id >= count) { p.end(); sendErr(400, "Bad id"); return; }
    for (int i = id; i < count - 1; i++) {
        String s  = p.getString(("ssid_" + String(i+1)).c_str(), "");
        String pw = p.getString(("pass_" + String(i+1)).c_str(), "");
        p.putString(("ssid_" + String(i)).c_str(), s);
        p.putString(("pass_" + String(i)).c_str(), pw);
    }
    p.remove(("ssid_" + String(count-1)).c_str());
    p.remove(("pass_" + String(count-1)).c_str());
    p.putInt("wifi_cnt", count - 1);
    p.end();
    for (int i = id; i < count - 1; i++) {
        WIFI_NETS[i] = WIFI_NETS[i+1];
    }
    WIFI_NET_COUNT = count - 1;
    sendOK();
}

// NEW: Move network at id up one slot (increase boot priority)
static void handleReorderWifi() {
    if (!server.hasArg("id")) { sendErr(400, "Missing id"); return; }
    int id = server.arg("id").toInt();
    Preferences p;
    p.begin("macro-kb", false);
    int count = p.getInt("wifi_cnt", 0);
    if (id <= 0 || id >= count) { p.end(); sendErr(400, "Bad id"); return; }
    String ssidA = p.getString(("ssid_" + String(id-1)).c_str(), "");
    String passA = p.getString(("pass_" + String(id-1)).c_str(), "");
    String ssidB = p.getString(("ssid_" + String(id)).c_str(), "");
    String passB = p.getString(("pass_" + String(id)).c_str(), "");
    p.putString(("ssid_" + String(id-1)).c_str(), ssidB);
    p.putString(("pass_" + String(id-1)).c_str(), passB);
    p.putString(("ssid_" + String(id)).c_str(),   ssidA);
    p.putString(("pass_" + String(id)).c_str(),   passA);
    p.end();
    WifiCred tmp    = WIFI_NETS[id-1];
    WIFI_NETS[id-1] = WIFI_NETS[id];
    WIFI_NETS[id]   = tmp;
    sendOK();
}

static void handleGetSettings() {
    String json = "{\"silentMode\":" + String(silentMode ? 1 : 0) +
                  ",\"sleepTimeout\":" + String(sleepTimeoutMs) + "}";
    sendJSON(200, json);
}

static void handleUpdateSettings() {
    if (!server.hasArg("silentMode") || !server.hasArg("sleepTimeout")) {
        sendErr(400, "Missing args"); return;
    }
    silentMode     = (server.arg("silentMode") == "1");
    sleepTimeoutMs = (unsigned long)server.arg("sleepTimeout").toInt();
    Preferences p;
    p.begin("macro-kb", false);
    p.putBool("silentMode",    silentMode);
    p.putULong("sleepTimeout", sleepTimeoutMs);
    p.end();
    sendOK();
}

static void handleReboot() {
    sendOK("Rebooting...");
    rebootRequested = true;
}

// BUG FIX: use rebootRequested flag instead of ESP.restart() inside WebServer task
static void handleUpdateComplete() {
    server.sendHeader("Connection", "close");
    server.sendHeader("Access-Control-Allow-Origin", "*");
    bool ok = !Update.hasError();
    server.send(200, "text/plain", ok ? "OTA Success! Rebooting..." : "OTA Failed");
    if (ok) {
        delay(300);
        rebootRequested = true;
    }
}

static void handleUpdateUpload() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) Update.printError(Serial);
    } else if (upload.status == UPLOAD_FILE_END) {
        if (!Update.end(true)) Update.printError(Serial);
    }
}

static void handleNotFound() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(404, "text/plain", "Not found");
}

static void webServerTask(void *pvParameters) {
    server.on("/",                    HTTP_GET,  handleRoot);
    server.on("/api/info",            HTTP_GET,  handleGetInfo);
    server.on("/api/wifi",            HTTP_GET,  handleGetWifi);
    server.on("/api/wifi/add",        HTTP_POST, handleAddWifi);
    server.on("/api/wifi/del",        HTTP_POST, handleDelWifi);
    server.on("/api/wifi/reorder",    HTTP_POST, handleReorderWifi);
    server.on("/api/settings",        HTTP_GET,  handleGetSettings);
    server.on("/api/settings/update", HTTP_POST, handleUpdateSettings);
    server.on("/api/reboot",          HTTP_POST, handleReboot);
    server.on("/update", HTTP_POST, handleUpdateComplete, handleUpdateUpload);
    server.onNotFound(handleNotFound);
    server.begin();
    serverRunning = true;
    while (serverRunning) {
        server.handleClient();
        vTaskDelay(5 / portTICK_PERIOD_MS);
    }
    server.stop();
    webServerTaskHandle = NULL;
    vTaskDelete(NULL);
}

void WebServer_Init() {
    rebootRequested = false;
    if (webServerTaskHandle == NULL) {
        serverRunning = false;
        xTaskCreatePinnedToCore(webServerTask, "WebServer", 8192, NULL, 1, &webServerTaskHandle, 1);
    }
}

void WebServer_Stop() {
    serverRunning = false;
    // BUG-28 FIX: Don't null webServerTaskHandle here.
    // The task self-nulls it on exit (see webServerTask). Setting it NULL here
    // before the task actually exits causes WebServer_Init() to spawn a SECOND
    // web server task if called quickly, causing two tasks fighting for port 80.
    // Just wait generously for the task to finish — it checks serverRunning every 5ms.
    vTaskDelay(200 / portTICK_PERIOD_MS);
    // Handle is already NULL'd by the task itself at this point.
}

bool WebServer_RebootRequested() {
    return rebootRequested;
}
