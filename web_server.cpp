
#include "web_server.h"
#include <WebServer.h>
#include <Preferences.h>
#include <Update.h>
#include "globals.h"

static WebServer server(80);
static TaskHandle_t webServerTaskHandle = NULL;
static volatile bool serverRunning = false;
static volatile bool rebootRequested = false;

extern Preferences prefs;

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
    @import url('https://fonts.googleapis.com/css2?family=Inter:wght@300;400;500;600;700&display=swap');
    *{margin:0;padding:0;box-sizing:border-box}
    :root{
      --bg:#0a0a0f;--surface:#13131a;--card:#1a1a24;--border:#2a2a3a;
      --accent:#7c6fe0;--accent2:#5b8af0;--danger:#e05c6f;--success:#4cd97b;
      --warn:#f0a84a;--text:#e8e8f0;--muted:#6b6b85;
    }
    body{font-family:'Inter',sans-serif;background:var(--bg);color:var(--text);min-height:100vh;padding:0}
    .topbar{background:linear-gradient(135deg,#1a1a2e,#16213e);border-bottom:1px solid var(--border);
      padding:18px 24px;display:flex;align-items:center;justify-content:space-between;
      box-shadow:0 4px 20px rgba(0,0,0,0.5)}
    .logo{display:flex;align-items:center;gap:12px}
    .logo-icon{width:38px;height:38px;background:linear-gradient(135deg,var(--accent),var(--accent2));
      border-radius:10px;display:flex;align-items:center;justify-content:center;font-size:18px}
    .logo h1{font-size:1.2rem;font-weight:700;background:linear-gradient(135deg,#a78bfa,#60a5fa);
      -webkit-background-clip:text;-webkit-text-fill-color:transparent}
    .status-pill{padding:6px 14px;border-radius:20px;font-size:0.75rem;font-weight:600;
      display:flex;align-items:center;gap:6px}
    .status-pill.connected{background:rgba(76,217,123,0.15);border:1px solid rgba(76,217,123,0.4);color:var(--success)}
    .status-pill.ap{background:rgba(240,168,74,0.15);border:1px solid rgba(240,168,74,0.4);color:var(--warn)}
    .status-dot{width:7px;height:7px;border-radius:50%;background:currentColor;animation:pulse 2s infinite}
    @keyframes pulse{0%,100%{opacity:1}50%{opacity:0.4}}
    .container{max-width:640px;margin:0 auto;padding:24px 16px}
    .section{margin-bottom:24px}
    .card{background:var(--card);border:1px solid var(--border);border-radius:16px;overflow:hidden;
      box-shadow:0 4px 24px rgba(0,0,0,0.3)}
    .card-header{padding:16px 20px;border-bottom:1px solid var(--border);
      display:flex;align-items:center;gap:10px;background:rgba(255,255,255,0.02)}
    .card-header .icon{width:32px;height:32px;border-radius:8px;display:flex;align-items:center;
      justify-content:center;font-size:15px}
    .card-header h2{font-size:0.95rem;font-weight:600;flex:1}
    .card-body{padding:20px}
    .net-list{display:flex;flex-direction:column;gap:8px;margin-bottom:16px}
    .net-item{display:flex;align-items:center;gap:12px;background:var(--surface);
      border:1px solid var(--border);border-radius:10px;padding:12px 14px;transition:all 0.2s}
    .net-item:hover{border-color:var(--accent);box-shadow:0 0 0 1px var(--accent) inset}
    .net-item .net-name{flex:1;font-size:0.9rem;font-weight:500}
    .net-item .net-idx{font-size:0.7rem;color:var(--muted);background:var(--border);
      padding:2px 8px;border-radius:10px;font-weight:600}
    .btn-del{background:rgba(224,92,111,0.15);border:1px solid rgba(224,92,111,0.3);
      color:var(--danger);border-radius:8px;padding:6px 12px;font-size:0.78rem;
      font-weight:600;cursor:pointer;transition:all 0.2s;white-space:nowrap}
    .btn-del:hover{background:var(--danger);color:#fff;border-color:var(--danger)}
    .empty-state{text-align:center;padding:20px;color:var(--muted);font-size:0.85rem}
    .divider{height:1px;background:var(--border);margin:16px 0}
    .form-row{display:flex;flex-direction:column;gap:10px}
    .input-group{display:flex;flex-direction:column;gap:6px}
    .input-group label{font-size:0.78rem;font-weight:500;color:var(--muted);text-transform:uppercase;letter-spacing:0.05em}
    input[type=text],input[type=password]{
      width:100%;padding:12px 14px;background:var(--surface);border:1px solid var(--border);
      border-radius:10px;color:var(--text);font-family:inherit;font-size:0.9rem;outline:none;transition:all 0.2s}
    input:focus{border-color:var(--accent);box-shadow:0 0 0 3px rgba(124,111,224,0.15)}
    .btn{padding:12px 20px;border-radius:10px;font-family:inherit;font-size:0.88rem;font-weight:600;
      cursor:pointer;border:none;transition:all 0.2s;width:100%;display:flex;align-items:center;justify-content:center;gap:8px}
    .btn-primary{background:linear-gradient(135deg,var(--accent),var(--accent2));color:#fff}
    .btn-primary:hover{transform:translateY(-1px);box-shadow:0 6px 20px rgba(124,111,224,0.4)}
    .btn-primary:active{transform:translateY(0)}
    .btn-danger{background:rgba(224,92,111,0.15);border:1px solid rgba(224,92,111,0.4);color:var(--danger)}
    .btn-danger:hover{background:var(--danger);color:#fff;border-color:var(--danger)}
    .btn-ota{background:linear-gradient(135deg,#7c3aed,#a855f7);color:#fff}
    .btn-ota:hover{transform:translateY(-1px);box-shadow:0 6px 20px rgba(168,85,247,0.4)}
    .toggle-row{display:flex;align-items:center;justify-content:space-between;padding:4px 0}
    .toggle-row label{font-size:0.9rem;font-weight:500}
    .toggle{position:relative;display:inline-block;width:48px;height:26px}
    .toggle input{opacity:0;width:0;height:0}
    .slider{position:absolute;inset:0;background:var(--border);border-radius:26px;cursor:pointer;transition:0.3s}
    .slider:before{content:'';position:absolute;width:20px;height:20px;bottom:3px;left:3px;
      background:#fff;border-radius:50%;transition:0.3s}
    input:checked+.slider{background:linear-gradient(135deg,var(--accent),var(--accent2))}
    input:checked+.slider:before{transform:translateX(22px)}
    select{width:100%;padding:12px 14px;background:var(--surface);border:1px solid var(--border);
      border-radius:10px;color:var(--text);font-family:inherit;font-size:0.9rem;outline:none;cursor:pointer}
    select:focus{border-color:var(--accent)}
    .file-input-wrap{border:2px dashed var(--border);border-radius:12px;padding:24px;text-align:center;
      transition:all 0.2s;cursor:pointer;position:relative}
    .file-input-wrap:hover{border-color:var(--accent);background:rgba(124,111,224,0.05)}
    .file-input-wrap input[type=file]{position:absolute;inset:0;opacity:0;cursor:pointer;width:100%;height:100%}
    .file-input-wrap .fi-icon{font-size:28px;margin-bottom:8px}
    .file-input-wrap p{font-size:0.85rem;color:var(--muted)}
    .file-name{margin-top:8px;font-size:0.82rem;color:var(--accent);font-weight:500}
    .progress-bar{height:6px;background:var(--border);border-radius:6px;overflow:hidden;margin-top:14px;display:none}
    .progress-bar-fill{height:100%;background:linear-gradient(90deg,var(--accent),var(--accent2));width:0;transition:width 0.3s;border-radius:6px}
    .toast{position:fixed;top:20px;right:20px;padding:12px 18px;border-radius:10px;font-size:0.85rem;font-weight:500;
      color:#fff;z-index:999;transform:translateX(120%);transition:transform 0.3s;max-width:280px}
    .toast.show{transform:translateX(0)}
    .toast.success{background:rgba(76,217,123,0.9);backdrop-filter:blur(10px)}
    .toast.error{background:rgba(224,92,111,0.9);backdrop-filter:blur(10px)}
    .toast.warn{background:rgba(240,168,74,0.9);backdrop-filter:blur(10px)}
    .info-grid{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-bottom:16px}
    .info-item{background:var(--surface);border:1px solid var(--border);border-radius:10px;padding:12px}
    .info-item .label{font-size:0.7rem;color:var(--muted);text-transform:uppercase;letter-spacing:0.05em;margin-bottom:4px}
    .info-item .value{font-size:0.9rem;font-weight:600;word-break:break-all}
  </style>
</head>
<body>
  <div class="topbar">
    <div class="logo">
      <div class="logo-icon">⌨️</div>
      <div><h1>Macro Keyboard</h1><p style="font-size:0.7rem;color:#6b6b85;margin-top:2px">Web Dashboard</p></div>
    </div>
    <div id="statusPill" class="status-pill connected"><span class="status-dot"></span><span id="statusText">Connected</span></div>
  </div>

  <div class="container">
    <!-- Device Info -->
    <div class="section">
      <div class="card">
        <div class="card-header">
          <div class="icon" style="background:rgba(124,111,224,0.2)">📡</div>
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
          <div class="icon" style="background:rgba(91,138,240,0.2)">📶</div>
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
              <input type="password" id="pass" placeholder="WiFi password" autocomplete="new-password">
            </div>
            <button class="btn btn-primary" onclick="addNetwork()">＋ Add WiFi Network</button>
          </div>
        </div>
      </div>
    </div>

    <!-- System Settings -->
    <div class="section">
      <div class="card">
        <div class="card-header">
          <div class="icon" style="background:rgba(76,217,123,0.2)">⚙️</div>
          <h2>System Settings</h2>
        </div>
        <div class="card-body">
          <div class="form-row">
            <div class="toggle-row">
              <label>Silent Mode (disable buzzer)</label>
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
          <div class="icon" style="background:rgba(224,92,111,0.2)">🔌</div>
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
          <div class="icon" style="background:rgba(168,85,247,0.2)">🔧</div>
          <h2>OTA Firmware Update</h2>
        </div>
        <div class="card-body">
          <form id="otaForm" method="POST" action="/update" enctype="multipart/form-data">
            <div class="file-input-wrap" onclick="document.getElementById('fwFile').click()">
              <input type="file" id="fwFile" name="update" accept=".bin" onchange="fileSelected(this)">
              <div class="fi-icon">📦</div>
              <p>Click to select firmware <strong>.bin</strong> file</p>
              <div class="file-name" id="fileName"></div>
            </div>
            <div class="progress-bar" id="progressBar"><div class="progress-bar-fill" id="progressFill"></div></div>
            <div style="margin-top:14px">
              <button type="button" class="btn btn-ota" onclick="uploadFirmware()">⬆️ Upload Firmware</button>
            </div>
          </form>
        </div>
      </div>
    </div>
  </div>

  <div class="toast" id="toast"></div>

  <script>
    function showToast(msg, type='success') {
      const t = document.getElementById('toast');
      t.textContent = msg; t.className = 'toast ' + type + ' show';
      setTimeout(() => { t.className = 'toast ' + type; }, 3000);
    }

    function fetchInfo() {
      fetch('/api/info').then(r=>r.json()).then(d => {
        document.getElementById('infoIP').textContent = d.ip || '—';
        document.getElementById('infoSSID').textContent = d.ssid || 'AP Mode';
        document.getElementById('infoSilent').textContent = d.silentMode ? 'ON' : 'OFF';
        const sleep = parseInt(d.sleepTimeout);
        document.getElementById('infoSleep').textContent = sleep === 0 ? 'Never' : sleep === 60000 ? '1 min' : sleep === 300000 ? '5 min' : '15 min';
        const pill = document.getElementById('statusPill');
        const txt = document.getElementById('statusText');
        if (d.ssid && d.ssid !== 'AP') { pill.className='status-pill connected'; txt.textContent='Connected'; }
        else { pill.className='status-pill ap'; txt.textContent='AP Mode'; }
      }).catch(() => {});
    }

    function fetchNetworks() {
      fetch('/api/wifi').then(r=>r.json()).then(data => {
        const el = document.getElementById('networks');
        if (!data.length) { el.innerHTML='<div class="empty-state">No saved networks. Add one below.</div>'; return; }
        el.innerHTML = data.map((n,i) =>
          `<div class="net-item">
            <span class="net-idx">#${i+1}</span>
            <span class="net-name">${n.ssid}</span>
            <button class="btn-del" onclick="delNetwork(${i})">Remove</button>
          </div>`
        ).join('');
      }).catch(() => { document.getElementById('networks').innerHTML='<div class="empty-state">Failed to load.</div>'; });
    }

    function addNetwork() {
      const ssid = document.getElementById('ssid').value.trim();
      const pass = document.getElementById('pass').value;
      if (!ssid) { showToast('Please enter a network name', 'error'); return; }
      fetch('/api/wifi/add', { method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'},
        body:'ssid='+encodeURIComponent(ssid)+'&pass='+encodeURIComponent(pass) })
        .then(r => {
          if (r.ok) { document.getElementById('ssid').value=''; document.getElementById('pass').value=''; fetchNetworks(); showToast('Network added!'); }
          else showToast('Failed to add network', 'error');
        }).catch(() => showToast('Connection error', 'error'));
    }

    function delNetwork(id) {
      if (!confirm('Remove this network?')) return;
      fetch('/api/wifi/del', { method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'},
        body:'id='+id })
        .then(r => { if(r.ok) { fetchNetworks(); showToast('Network removed', 'warn'); } })
        .catch(() => showToast('Failed to remove', 'error'));
    }

    function fetchSettings() {
      fetch('/api/settings').then(r=>r.json()).then(d => {
        document.getElementById('silentMode').checked = (d.silentMode == 1);
        document.getElementById('sleepTimeout').value = d.sleepTimeout;
      }).catch(() => {});
    }

    function saveSettings() {
      const sil = document.getElementById('silentMode').checked ? 1 : 0;
      const slp = document.getElementById('sleepTimeout').value;
      fetch('/api/settings/update', { method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'},
        body:'silentMode='+sil+'&sleepTimeout='+slp })
        .then(r => { if(r.ok) { showToast('Settings saved!'); fetchInfo(); } else showToast('Failed to save', 'error'); })
        .catch(() => showToast('Connection error', 'error'));
    }

    function rebootDevice() {
      if (!confirm('Reboot the keyboard now?')) return;
      fetch('/api/reboot', { method:'POST' })
        .then(() => showToast('Rebooting... please reconnect.', 'warn'))
        .catch(() => showToast('Reboot command sent', 'warn'));
    }

    function fileSelected(input) {
      document.getElementById('fileName').textContent = input.files[0] ? input.files[0].name : '';
    }

    function uploadFirmware() {
      const file = document.getElementById('fwFile').files[0];
      if (!file) { showToast('Please select a .bin file first', 'error'); return; }
      if (!file.name.endsWith('.bin')) { showToast('File must be a .bin firmware', 'error'); return; }
      const bar = document.getElementById('progressBar');
      const fill = document.getElementById('progressFill');
      bar.style.display='block'; fill.style.width='0%';
      const fd = new FormData();
      fd.append('update', file);
      const xhr = new XMLHttpRequest();
      xhr.upload.onprogress = e => { if (e.lengthComputable) fill.style.width=(e.loaded/e.total*100)+'%'; };
      xhr.onload = () => {
        if (xhr.status === 200) { fill.style.width='100%'; showToast('Upload complete! Rebooting...'); }
        else showToast('OTA Failed: ' + xhr.responseText, 'error');
      };
      xhr.onerror = () => showToast('Upload failed — connection lost', 'error');
      xhr.open('POST', '/update'); xhr.send(fd);
    }

    // Init
    fetchInfo(); fetchNetworks(); fetchSettings();
    setInterval(fetchInfo, 5000);
  </script>
</body>
</html>
)rawliteral";

// ─────────────────────────────────────────────────────────────────────────────
// API Handlers (all run in the WebServer task — no cross-task globals mutation)
// ─────────────────────────────────────────────────────────────────────────────

static void handleRoot() {
    server.send_P(200, "text/html", index_html);
}

static void handleGetInfo() {
    bool isSTA = (WiFi.status() == WL_CONNECTED);
    String ip   = isSTA ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
    String ssid = isSTA ? WiFi.SSID() : String("AP");
    // Read settings live (same task — safe)
    bool   sm  = silentMode;
    unsigned long slp = sleepTimeoutMs;
    String json = "{\"ip\":\"" + ip + "\",\"ssid\":\"" + ssid +
                  "\",\"silentMode\":" + (sm ? "1" : "0") +
                  ",\"sleepTimeout\":" + String(slp) + "}";
    server.send(200, "application/json", json);
}

static void handleGetWifi() {
    // Read from flash — safe to do in any task
    Preferences p;
    p.begin("macro-kb", true); // read-only
    int count = p.getInt("wifi_cnt", 0);
    String json = "[";
    for (int i = 0; i < count; i++) {
        String ssid = p.getString(("ssid_" + String(i)).c_str(), "");
        if (i > 0) json += ",";
        json += "{\"id\":" + String(i) + ",\"ssid\":\"" + ssid + "\"}";
    }
    json += "]";
    p.end();
    server.send(200, "application/json", json);
}

static void handleAddWifi() {
    if (!server.hasArg("ssid")) { server.send(400, "text/plain", "Missing ssid"); return; }
    String ssid = server.arg("ssid");
    String pass = server.arg("pass");
    ssid.trim();
    if (ssid.length() == 0) { server.send(400, "text/plain", "Empty ssid"); return; }

    Preferences p;
    p.begin("macro-kb", false);
    int count = p.getInt("wifi_cnt", 0);
    if (count >= 10) { p.end(); server.send(400, "text/plain", "Max 10 networks"); return; }
    p.putString(("ssid_" + String(count)).c_str(), ssid);
    p.putString(("pass_" + String(count)).c_str(), pass);
    p.putInt("wifi_cnt", count + 1);
    p.end();

    // Update live in-memory array too (safe write to simple global array)
    if (count < 10) {
        strncpy(WIFI_NETS[count].ssid, ssid.c_str(), 32);
        WIFI_NETS[count].ssid[32] = '\0';
        strncpy(WIFI_NETS[count].pass, pass.c_str(), 64);
        WIFI_NETS[count].pass[64] = '\0';
        WIFI_NET_COUNT = count + 1;
    }
    server.send(200, "text/plain", "OK");
}

static void handleDelWifi() {
    if (!server.hasArg("id")) { server.send(400, "text/plain", "Missing id"); return; }
    int id    = server.arg("id").toInt();

    Preferences p;
    p.begin("macro-kb", false);
    int count = p.getInt("wifi_cnt", 0);
    if (id < 0 || id >= count) { p.end(); server.send(400, "text/plain", "Bad id"); return; }

    // Shift remaining entries down
    for (int i = id; i < count - 1; i++) {
        String s = p.getString(("ssid_" + String(i+1)).c_str(), "");
        String pw = p.getString(("pass_" + String(i+1)).c_str(), "");
        p.putString(("ssid_" + String(i)).c_str(), s);
        p.putString(("pass_" + String(i)).c_str(), pw);
    }
    p.remove(("ssid_" + String(count-1)).c_str());
    p.remove(("pass_" + String(count-1)).c_str());
    p.putInt("wifi_cnt", count - 1);
    p.end();

    // Update live array
    for (int i = id; i < count - 1; i++) {
        WIFI_NETS[i] = WIFI_NETS[i+1];
    }
    WIFI_NET_COUNT = count - 1;
    server.send(200, "text/plain", "OK");
}

static void handleGetSettings() {
    String json = "{\"silentMode\":" + String(silentMode ? 1 : 0) +
                  ",\"sleepTimeout\":" + String(sleepTimeoutMs) + "}";
    server.send(200, "application/json", json);
}

static void handleUpdateSettings() {
    if (!server.hasArg("silentMode") || !server.hasArg("sleepTimeout")) {
        server.send(400, "text/plain", "Missing args");
        return;
    }
    silentMode    = (server.arg("silentMode") == "1");
    sleepTimeoutMs = (unsigned long)server.arg("sleepTimeout").toInt();

    Preferences p;
    p.begin("macro-kb", false);
    p.putBool("silentMode", silentMode);
    p.putULong("sleepTimeout", sleepTimeoutMs);
    p.end();
    server.send(200, "text/plain", "OK");
}

static void handleReboot() {
    server.send(200, "text/plain", "Rebooting...");
    rebootRequested = true; // handled safely in main task context
}

static void handleUpdateComplete() {
    server.sendHeader("Connection", "close");
    bool ok = !Update.hasError();
    server.send(200, "text/plain", ok ? "OTA Success! Rebooting..." : "OTA Failed");
    if (ok) {
        delay(500);
        ESP.restart();
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
    server.send(404, "text/plain", "Not found");
}

static void webServerTask(void *pvParameters) {
    server.on("/",                   HTTP_GET,  handleRoot);
    server.on("/api/info",           HTTP_GET,  handleGetInfo);
    server.on("/api/wifi",           HTTP_GET,  handleGetWifi);
    server.on("/api/wifi/add",       HTTP_POST, handleAddWifi);
    server.on("/api/wifi/del",       HTTP_POST, handleDelWifi);
    server.on("/api/settings",       HTTP_GET,  handleGetSettings);
    server.on("/api/settings/update",HTTP_POST, handleUpdateSettings);
    server.on("/api/reboot",         HTTP_POST, handleReboot);
    server.on("/update",  HTTP_POST, handleUpdateComplete, handleUpdateUpload);
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
        serverRunning = false; // task sets it true after server.begin()
        xTaskCreatePinnedToCore(webServerTask, "WebServer", 8192, NULL, 1, &webServerTaskHandle, 1);
    }
}

void WebServer_Stop() {
    serverRunning = false;
    // Give the task time to exit cleanly
    vTaskDelay(100 / portTICK_PERIOD_MS);
    webServerTaskHandle = NULL;
}

bool WebServer_RebootRequested() {
    return rebootRequested;
}
