#include "web_server.h"
#include <WebServer.h>
#include <Preferences.h>
#include "globals.h"

static WebServer server(80);
static TaskHandle_t webServerTaskHandle = NULL;
static bool serverRunning = false;

extern Preferences prefs;

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Macro Keyboard Dashboard</title>
  <style>
    body { font-family: 'Inter', sans-serif; background-color: #121212; color: #fff; margin: 0; padding: 20px; }
    h1, h2 { color: #00d2ff; }
    .card { background: #1e1e1e; padding: 20px; border-radius: 12px; margin-bottom: 20px; box-shadow: 0 4px 10px rgba(0,0,0,0.5); }
    input[type=text], input[type=password], select { width: 100%; padding: 12px; margin: 8px 0; border: none; border-radius: 6px; background: #2c2c2c; color: white; box-sizing: border-box; }
    button { background: #00d2ff; color: #000; border: none; padding: 12px 20px; text-transform: uppercase; font-weight: bold; border-radius: 6px; cursor: pointer; width: 100%; box-sizing: border-box; }
    button:hover { background: #3a86ff; color: #fff; }
    .net-item { display: flex; justify-content: space-between; align-items: center; background: #2c2c2c; padding: 10px; border-radius: 6px; margin-bottom: 8px; }
    .net-item button { width: auto; padding: 6px 12px; background: #ff3333; color: white; }
    .net-item button:hover { background: #ff0000; }
  </style>
</head>
<body>
  <h1>Macro Keyboard Setup</h1>
  
  <div class="card">
    <h2>WiFi Networks</h2>
    <div id="networks">Loading...</div>
    <h3>Add Network</h3>
    <input type="text" id="ssid" placeholder="WiFi Name (SSID)">
    <input type="password" id="pass" placeholder="Password">
    <button onclick="addNetwork()">Add WiFi</button>
  </div>

  <div class="card">
    <h2>System Settings</h2>
    <label>Silent Mode:</label>
    <select id="silentMode">
      <option value="0">OFF</option>
      <option value="1">ON</option>
    </select>
    <label>Display Timeout:</label>
    <select id="sleepTimeout">
      <option value="60000">1 Minute</option>
      <option value="300000">5 Minutes</option>
      <option value="900000">15 Minutes</option>
      <option value="0">Never</option>
    </select>
    <button onclick="saveSettings()">Save Settings</button>
  </div>

  <div class="card" style="border-left: 4px solid #ff3333;">
    <h2 style="color: #ff3333;">Power Options</h2>
    <button style="background: #ff3333; color: white;" onclick="rebootDevice()">Reboot Keyboard</button>
  </div>

  <script>
    function fetchNetworks() {
      fetch('/api/wifi').then(r=>r.json()).then(data => {
        let h = '';
        data.forEach((n, i) => {
          h += `<div class="net-item"><span>${n.ssid}</span><button onclick="delNetwork(${i})">X</button></div>`;
        });
        document.getElementById('networks').innerHTML = h || 'No saved networks.';
      });
    }
    function addNetwork() {
      const ssid = document.getElementById('ssid').value;
      const pass = document.getElementById('pass').value;
      if (!ssid) return;
      fetch('/api/wifi/add', { method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'}, body:`ssid=${encodeURIComponent(ssid)}&pass=${encodeURIComponent(pass)}` })
        .then(() => { document.getElementById('ssid').value = ''; document.getElementById('pass').value = ''; fetchNetworks(); });
    }
    function delNetwork(id) {
      fetch('/api/wifi/del', { method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'}, body:`id=${id}` })
        .then(() => fetchNetworks());
    }
    function fetchSettings() {
      fetch('/api/settings').then(r=>r.json()).then(data => {
        document.getElementById('silentMode').value = data.silentMode;
        document.getElementById('sleepTimeout').value = data.sleepTimeout;
      });
    }
    function saveSettings() {
      const sil = document.getElementById('silentMode').value;
      const slp = document.getElementById('sleepTimeout').value;
      fetch('/api/settings/update', { method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'}, body:`silentMode=${sil}&sleepTimeout=${slp}` })
        .then(() => alert('Settings saved! Reboot for network changes to apply.'));
    }
    function rebootDevice() {
      if (confirm('Are you sure you want to reboot the keyboard?')) {
        fetch('/api/reboot', { method:'POST' }).then(() => alert('Rebooting...'));
      }
    }
    fetchNetworks(); fetchSettings();
  </script>
</body>
</html>
)rawliteral";

static void handleRoot() {
    server.send(200, "text/html", index_html);
}

static void handleGetWifi() {
    String json = "[";
    int count = prefs.getInt("wifi_cnt", 0);
    for(int i=0; i<count; i++) {
        String ssid = prefs.getString(("ssid_" + String(i)).c_str(), "");
        json += "{\"id\":" + String(i) + ",\"ssid\":\"" + ssid + "\"}";
        if (i < count-1) json += ",";
    }
    json += "]";
    server.send(200, "application/json", json);
}

static void handleAddWifi() {
    if (server.hasArg("ssid") && server.hasArg("pass")) {
        String ssid = server.arg("ssid");
        String pass = server.arg("pass");
        int count = prefs.getInt("wifi_cnt", 0);
        prefs.putString(("ssid_" + String(count)).c_str(), ssid);
        prefs.putString(("pass_" + String(count)).c_str(), pass);
        prefs.putInt("wifi_cnt", count + 1);
        server.send(200, "text/plain", "OK");
    } else {
        server.send(400, "text/plain", "Missing args");
    }
}

static void handleDelWifi() {
    if (server.hasArg("id")) {
        int id = server.arg("id").toInt();
        int count = prefs.getInt("wifi_cnt", 0);
        
        // Shift remaining down
        for (int i = id; i < count - 1; i++) {
            prefs.putString(("ssid_" + String(i)).c_str(), prefs.getString(("ssid_" + String(i+1)).c_str(), ""));
            prefs.putString(("pass_" + String(i)).c_str(), prefs.getString(("pass_" + String(i+1)).c_str(), ""));
        }
        
        if (count > 0) {
            prefs.remove(("ssid_" + String(count - 1)).c_str());
            prefs.remove(("pass_" + String(count - 1)).c_str());
            prefs.putInt("wifi_cnt", count - 1);
        }
        server.send(200, "text/plain", "OK");
    } else {
        server.send(400, "text/plain", "Missing id");
    }
}

static void handleGetSettings() {
    String json = "{\"silentMode\":" + String(silentMode ? 1 : 0) + ",\"sleepTimeout\":" + String(sleepTimeoutMs) + "}";
    server.send(200, "application/json", json);
}

static void handleUpdateSettings() {
    if (server.hasArg("silentMode") && server.hasArg("sleepTimeout")) {
        silentMode = (server.arg("silentMode") == "1");
        sleepTimeoutMs = (unsigned long)server.arg("sleepTimeout").toInt();
        prefs.putBool("silentMode", silentMode);
        prefs.putULong("sleepTimeout", sleepTimeoutMs);
        server.send(200, "text/plain", "OK");
    } else {
        server.send(400, "text/plain", "Missing args");
    }
}

static void handleReboot() {
    server.send(200, "text/plain", "Rebooting...");
    delay(500);
    ESP.restart();
}

static void webServerTask(void *pvParameters) {
    server.on("/", handleRoot);
    server.on("/api/wifi", HTTP_GET, handleGetWifi);
    server.on("/api/wifi/add", HTTP_POST, handleAddWifi);
    server.on("/api/wifi/del", HTTP_POST, handleDelWifi);
    server.on("/api/settings", HTTP_GET, handleGetSettings);
    server.on("/api/settings/update", HTTP_POST, handleUpdateSettings);
    server.on("/api/reboot", HTTP_POST, handleReboot);
    
    server.begin();
    serverRunning = true;
    
    while(serverRunning) {
        server.handleClient();
        delay(10); // Yield to other tasks
    }
    
    server.stop();
    webServerTaskHandle = NULL;
    vTaskDelete(NULL);
}

void WebServer_Init() {
    if (webServerTaskHandle == NULL) {
        serverRunning = true; // prevent race condition before task starts
        xTaskCreatePinnedToCore(webServerTask, "WebServer", 4096, NULL, 1, &webServerTaskHandle, 1);
    }
}

void WebServer_Stop() {
    if (serverRunning) {
        serverRunning = false;
        // Task will delete itself
    }
}
