#include "app_wifi.h"
#include <esp_wifi.h>

// ── WIFI ANALYZER STATE ──────────────────────────────────
bool inWifiMode = false;
int wifiView = 0; // 0=Scanner, 1=Target RSSI, 2=Target Traffic
int wifiSel = 0; // Selected network index
int wifiCount = 0; // Scanned network count
char targetSSID[33] = {0};
uint8_t targetBSSID[6];
int targetChannel = 1;

#define GRAPH_W 128
int8_t dbmHistory[GRAPH_W];     // 0 = empty sentinel
int trafficHistory[GRAPH_W];    // -1 = empty sentinel
int graphIdx = 0;
volatile int packetCount = 0;
int rawKbps = 0; // Latest unsmoothed KB/s reading for live display

// ── CLIENT DEVICE TRACKER ────────────────────────────────────────────────
// Counts unique client devices seen via 802.11 data-frame source MACs.
// Only populated in wifiView == 2 (promiscuous / traffic mode).
// Reset on every entry into traffic mode for a fresh session count.
#define MAX_DEVICE_TRACK 32
uint8_t  deviceMACs[MAX_DEVICE_TRACK][6]; // unique 6-byte MACs seen this session
volatile int deviceCount = 0;             // number of unique clients detected



// ── WIFI ANALYZER MODE ────────────────────────────────────

void IRAM_ATTR sniffer_callback(void* buf, wifi_promiscuous_pkt_type_t type) {
  if (type == WIFI_PKT_MISC) return;
  wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
  int pktLen = pkt->rx_ctrl.sig_len;
  packetCount += pktLen;

  // ── Client device counting ─────────────────────────────────────────────
  if (pktLen < 24) return;                    // too short for 802.11 MAC header
  uint8_t* f = pkt->payload;
  uint8_t frameType = (f[0] & 0x0C) >> 2;    // 0=Mgmt 1=Ctrl 2=Data
  if (frameType != 2) return;                 // data frames only

  uint8_t toDS   =  (f[1] & 0x01);
  uint8_t fromDS = ((f[1] & 0x02) >> 1);

  // 802.11 infrastructure-mode address roles:
  //   TO-DS=1, FROM-DS=0  (STA→AP):  addr1 = BSSID, addr2 = Client
  //   FROM-DS=1, TO-DS=0  (AP→STA):  addr1 = Client, addr2 = BSSID
  uint8_t* clientMac;
  uint8_t* apMac;
  
  if (toDS && !fromDS) {
    apMac = f + 4;         // addr1 is AP
    clientMac = f + 10;    // addr2 is STA
  } else if (!toDS && fromDS) {
    clientMac = f + 4;     // addr1 is STA
    apMac = f + 10;        // addr2 is AP
  } else {
    return;                // IBSS / WDS — skip
  }

  // CRITICAL: Only count devices that are talking to OUR selected AP!
  if (memcmp(apMac, targetBSSID, 6) != 0) return; 

  if (clientMac[0] & 0x01) return; // exclude broadcast/multicast

  int cnt = deviceCount;                     // volatile snapshot
  if (cnt >= MAX_DEVICE_TRACK) return;
  for (int i = 0; i < cnt; i++) {
    if (memcmp(deviceMACs[i], clientMac, 6) == 0) return; // already known
  }
  memcpy(deviceMACs[cnt], clientMac, 6);           // store before bumping count
  deviceCount = cnt + 1;
}

unsigned long wifiLastTick = 0;

void startWifiAnalyzer() {
  inWifiMode = true;
  wifiView = 0;
  wifiSel = 0;
  wifiCount = 0;
  wifiLastTick = 0;
  
  // Prepare WiFi (Radio is already activated by AppContainer transition)
  
  WiFi.scanNetworks(true); // Start async scan
}

#include "app_manager.h"

void exitWifiAnalyzer() {
  inWifiMode = false;
  // Ensure promiscuous mode is off
  esp_wifi_set_promiscuous(false);
  
  WiFi.scanDelete(); // Free memory from scanner
}

void drawWifiScanner() {
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) {
    centered("Scanning 2.4GHz...", 28);
  } else if (n == WIFI_SCAN_FAILED) {
    centered("Scan Failed", 28);
    WiFi.scanNetworks(true);
  } else if (n == 0) {
    centered("No Networks Found", 28);
    WiFi.scanNetworks(true);
  } else {
    wifiCount = n;
    if (wifiSel >= wifiCount) wifiSel = wifiCount - 1;
    if (wifiSel < 0) wifiSel = 0;
    
    // Draw Header
    oled.fillRect(0, 0, SCREEN_W, 10, SSD1306_WHITE);
    oled.setTextColor(SSD1306_BLACK);
    oled.setCursor(2, 1);
    oled.print("WIFI SCANNER [");
    oled.print(wifiCount);
    oled.print("]");
    
    oled.setTextColor(SSD1306_WHITE);
    
    // Draw List (show max 6 items)
    int startIdx = max(0, wifiSel - 3);
    for (int i = 0; i < 6; i++) {
      int idx = startIdx + i;
      if (idx >= wifiCount) break;
      
      int y = 12 + (i * 9);
      if (idx == wifiSel) {
        oled.setCursor(0, y);
        oled.print(">");
      }
      
      oled.setCursor(10, y);
      oled.print(WiFi.RSSI(idx));
      oled.print(" ");
      String ssid = WiFi.SSID(idx);
      if (ssid.length() > 14) ssid = ssid.substring(0, 14);
      oled.print(ssid);
    }
  }
  oled.display();
}

void drawWifiTarget() {
  oled.clearDisplay();
  
  // Header
  oled.fillRect(0, 0, SCREEN_W, 10, SSD1306_WHITE);
  oled.setTextColor(SSD1306_BLACK);
  oled.setCursor(2, 1);
  oled.print("CH");
  oled.print(targetChannel);
  oled.print(" ");
  char s[14] = {0};
  strncpy(s, targetSSID, 13);
  oled.print(s);
  
  oled.setTextColor(SSD1306_WHITE);
  
  // Draw Graph — find max value for auto-scaling
  int maxVal = 0; // traffic starts from 0; dBm uses negative values
  int minVal = -30; // dBm best case
  for (int i = 0; i < GRAPH_W; i++) {
    if (wifiView == 1) {
      if (dbmHistory[i] > maxVal && dbmHistory[i] < 0) maxVal = dbmHistory[i];
      if (dbmHistory[i] < minVal && dbmHistory[i] < 0) minVal = dbmHistory[i];
    } else {
      // Only count real (non-sentinel) values for auto-scale
      if (trafficHistory[i] != -1 && trafficHistory[i] > maxVal) maxVal = trafficHistory[i];
    }
  }
  
  // Baseline and axes
  oled.drawFastHLine(0, 60, GRAPH_W, SSD1306_WHITE);
  oled.drawFastVLine(0, 12, 48, SSD1306_WHITE);
  
  // Plot
  for (int i = 0; i < GRAPH_W - 1; i++) {
    int v1 = (wifiView == 1) ? dbmHistory[i] : trafficHistory[i];
    int v2 = (wifiView == 1) ? dbmHistory[i+1] : trafficHistory[i+1];
    
    // Skip if either point is an empty sentinel
    if (wifiView == 1 && (v1 == 0 || v2 == 0)) continue;
    if (wifiView == 2 && (v1 == -1 || v2 == -1)) continue;
    
    int y1, y2;
    if (wifiView == 1) { // dBm graph (-100 to -30 dBm)
      y1 = map(constrain(v1, -100, -30), -100, -30, 59, 12);
      y2 = map(constrain(v2, -100, -30), -100, -30, 59, 12);
    } else { // Traffic graph auto-scaled to peak
      int scale = maxVal < 1 ? 1 : maxVal;
      y1 = map(constrain(v1, 0, scale), 0, scale, 59, 12);
      y2 = map(constrain(v2, 0, scale), 0, scale, 59, 12);
    }
    
    oled.drawLine(i, y1, i+1, y2, SSD1306_WHITE);
  }
  
  oled.display();
}

void drawWifiDisplay2() {
  if (!oled2Active) return;
  // Right safe zone: X=73–107 = 35px wide. Dead cols at 72 and 108.
  // Safe Y: 0–54. Size-1 char = 6×8px. Size-2 char = 12×16px.
  // Centering formula: X = 73 + max(0, (35 - textWidthPx) / 2)
  oled2.clearDisplay();
  oled2.setTextColor(SSD1306_WHITE);
  d2Divider();  // vertical separator at X=71

  if (wifiView == 0) {
    // ── SCANNER ───────────────────────────────────────────────────────────
    // Left : selected network details + position indicator
    // Right: big total-networks count (centered) + "NETS" label
    int n = WiFi.scanComplete();
    bool hasResults = (n > 0 && n != WIFI_SCAN_RUNNING && n != WIFI_SCAN_FAILED);
    d2Header("WIFI", hasResults ? "SCAN" : "....");
    oled2.setTextSize(1);

    if (!hasResults) {
      d2L(22, "SCANNING");
      d2L(34, "2.4GHz..");
    } else {
      int sel = constrain(wifiSel, 0, n - 1);

      // Left: SSID / RSSI / channel / position
      char ssidBuf[9] = {0};
      strncpy(ssidBuf, WiFi.SSID(sel).c_str(), 8);
      d2L(16, ssidBuf);

      char rBuf[8];
      snprintf(rBuf, sizeof(rBuf), "%ddBm", WiFi.RSSI(sel));
      d2L(26, rBuf);

      char chBuf[7];
      snprintf(chBuf, sizeof(chBuf), "CH: %d", WiFi.channel(sel));
      d2L(36, chBuf);

      char posBuf[8];   // e.g. "3/12" — position in list
      snprintf(posBuf, sizeof(posBuf), "%d/%d", sel + 1, min(n, 99));
      d2L(46, posBuf);

      // Right: net count, size-2, centered
      char cntBuf[4];
      snprintf(cntBuf, sizeof(cntBuf), "%d", min(n, 99));
      int cntW = strlen(cntBuf) * 12;
      oled2.setTextSize(2);
      oled2.setCursor(73 + max(0, (35 - cntW) / 2), 18);
      oled2.print(cntBuf);

      // "NETS" label, size-1, centered
      oled2.setTextSize(1);
      int lblW = 4 * 6;  // "NETS" = 24px
      oled2.setCursor(73 + max(0, (35 - lblW) / 2), 40);
      oled2.print("NETS");
    }

  } else if (wifiView == 1) {
    // ── RSSI MONITOR ───────────────────────────────────────────────────
    // Left : SSID / channel / DEVICE label
    // Right: RSSI (centered, size-1) | divider line | device count (size-2, BIG)
    char ts[9]; getTimeCStr(ts);
    d2Header(ts, "RSSI");
    oled2.setTextSize(1);

    // Left
    char tSSID[9] = {0};
    strncpy(tSSID, targetSSID, 8);
    d2L(16, tSSID);
    char chBuf[7];
    snprintf(chBuf, sizeof(chBuf), "CH: %d", targetChannel);
    d2L(26, chBuf);
    d2L(36, "DEVICE:");

    // Right top: RSSI value, size-1, centered
    char rssiStr[7];
    snprintf(rssiStr, sizeof(rssiStr), "%d", (int)dbmHistory[GRAPH_W - 1]);
    int rssiW = strlen(rssiStr) * 6;
    oled2.setCursor(73 + max(0, (35 - rssiW) / 2), 16);
    oled2.print(rssiStr);

    // "dBm" label, size-1, centered
    oled2.setCursor(73 + max(0, (35 - 18) / 2), 25);  // "dBm"=3×6=18px
    oled2.print("dBm");

    // Thin separator across right zone
    oled2.drawFastHLine(74, 34, 33, SSD1306_WHITE);

    // Right bottom: device count, size-2, centered
    // Y=38 → bottom at Y=54 (safe boundary — exact fit)
    char devStr[4];
    snprintf(devStr, sizeof(devStr), "%d", (int)deviceCount);
    int devW = strlen(devStr) * 12;
    oled2.setTextSize(2);
    oled2.setCursor(73 + max(0, (35 - devW) / 2), 38);
    oled2.print(devStr);
    oled2.setTextSize(1);

  } else { // wifiView == 2
    // ── TRAFFIC MONITOR ──────────────────────────────────────────────────
    // Left : SSID / live traffic rate / DEVICE label
    // Right: speed (centered, size-1) | divider line | device count (size-2, BIG)
    char ts[9]; getTimeCStr(ts);
    d2Header(ts, "TRAFF");
    oled2.setTextSize(1);

    int kbps = rawKbps < 0 ? 0 : rawKbps;

    // Left
    char tSSID[9] = {0};
    strncpy(tSSID, targetSSID, 8);
    d2L(16, tSSID);

    char trafBuf[9];
    if (kbps >= 1024) snprintf(trafBuf, sizeof(trafBuf), "%.1fMB/s", kbps / 1024.0f);
    else              snprintf(trafBuf, sizeof(trafBuf), "%dKB/s",   kbps);
    d2L(26, trafBuf);
    d2L(36, "DEVICE:");

    // Right top: speed value + unit, size-1, both centered independently
    char speedVal[7], speedUnit[5];
    if (kbps >= 1024) {
      snprintf(speedVal, sizeof(speedVal), "%.1f", kbps / 1024.0f);
      strcpy(speedUnit, "MB/s");
    } else {
      snprintf(speedVal, sizeof(speedVal), "%d", kbps);
      strcpy(speedUnit, "KB/s");
    }
    int svalW  = strlen(speedVal)  * 6;
    int sunitW = strlen(speedUnit) * 6;
    oled2.setCursor(73 + max(0, (35 - svalW)  / 2), 16);
    oled2.print(speedVal);
    oled2.setCursor(73 + max(0, (35 - sunitW) / 2), 25);
    oled2.print(speedUnit);

    // Thin separator across right zone
    oled2.drawFastHLine(74, 34, 33, SSD1306_WHITE);

    // Right bottom: device count, size-2, centered
    // Y=38 → bottom at Y=54 (safe boundary — exact fit)
    char devStr[4];
    snprintf(devStr, sizeof(devStr), "%d", (int)deviceCount);
    int devW = strlen(devStr) * 12;
    oled2.setTextSize(2);
    oled2.setCursor(73 + max(0, (35 - devW) / 2), 38);
    oled2.print(devStr);
    oled2.setTextSize(1);
  }

  oled2.display();
}

// ── RENDERING OPTIMIZATION ─────────────────────────────────
static bool needsRedraw = true;
static int lastScanStatus = -99;
static unsigned long rssiLastTick = 0;

void updateWifiAnalyzer() {
  if (wifiView == 0) {
    // 1. Scanner Mode (Async, only redraws on changes)
    int n = WiFi.scanComplete();
    if (n != lastScanStatus) {
      lastScanStatus = n;
      needsRedraw = true;
    }
    
    if (needsRedraw) {
      needsRedraw = false;
      drawWifiDisplay2();
      drawWifiScanner();
    }
  }
  else if (wifiView == 1) {
    // 2. Target RSSI Mode (Fully Asynchronous, Non-Blocking)
    int n = WiFi.scanComplete();
    
    // Only process if the scan is finished (or failed)
    if (n != WIFI_SCAN_RUNNING) {
      if (millis() - rssiLastTick >= 1000) {
        rssiLastTick = millis();
        
        int currentRSSI = -100;
        if (n > 0) {
          for (int i = 0; i < n; i++) {
            if (WiFi.SSID(i) == targetSSID) {
              currentRSSI = WiFi.RSSI(i);
              break;
            }
          }
        }
        
        WiFi.scanDelete(); // Free previous scan memory
        
        // Shift graph left using highly optimized memmove
        memmove(dbmHistory, dbmHistory + 1, GRAPH_W - 1);
        dbmHistory[GRAPH_W - 1] = currentRSSI;
        
        needsRedraw = true;
        
        // Start next async scan targeted on this channel
        WiFi.scanNetworks(true, false, false, 100, targetChannel);
      }
    }
    
    if (needsRedraw) {
      needsRedraw = false;
      drawWifiDisplay2();
      drawWifiTarget();
    }
  }
  else if (wifiView == 2) {
    // 3. Traffic Sniffer Mode (1-Second Windows)
    if (millis() - wifiLastTick >= 1000) {
      wifiLastTick = millis();
      
      int currentBytes = packetCount;
      packetCount = 0; // reset for next 1-second window
      
      rawKbps = currentBytes / 1024; // Keep raw for live big-number display
      
      int prevKbps = rawKbps;
      for (int i = GRAPH_W - 1; i >= 0; i--) {
        if (trafficHistory[i] != -1) { prevKbps = trafficHistory[i]; break; }
      }
      int smoothedKbps = (prevKbps * 1 + rawKbps * 2) / 3; // 33% old, 67% new
      
      // Shift graph left and append smoothed value
      memmove(trafficHistory, trafficHistory + 1, (GRAPH_W - 1) * sizeof(int));
      trafficHistory[GRAPH_W - 1] = smoothedKbps;
      
      needsRedraw = true;
    }
    
    if (needsRedraw) {
      needsRedraw = false;
      drawWifiDisplay2();
      drawWifiTarget();
    }
  }
}



void AppWifi_Init() {
    startWifiAnalyzer();
}

void AppWifi_Update() {
    updateWifiAnalyzer();
}

void AppWifi_Exit() {
    exitWifiAnalyzer();
}

void AppWifi_HandleEvent(LogicalEvent ev) {
  if (!inWifiMode) return;

  if (ev == EV_UP_TAP) {
    if (wifiView == 0) { wifiSel = max(0, wifiSel - 1); needsRedraw = true; }
  }
  else if (ev == EV_DOWN_TAP) {
    if (wifiView == 0) { wifiSel++; needsRedraw = true; }
  }
  else if (ev == EV_CENTER_TAP) {
    if (wifiView == 0) {
      int n = WiFi.scanComplete();
      if (n > 0 && wifiSel < n) {
        uint8_t* bssid = WiFi.BSSID(wifiSel);
        if (bssid != nullptr) {
          // Scanner ➡️ RSSI
          memcpy(targetBSSID, bssid, 6);
          strncpy(targetSSID, WiFi.SSID(wifiSel).c_str(), 32);
          targetSSID[32] = '\0';
          targetChannel = WiFi.channel(wifiSel);
          wifiView = 1;
          for(int i = 0; i < GRAPH_W; i++) { dbmHistory[i] = 0; trafficHistory[i] = -1; }
          packetCount = 0;
          deviceCount = 0;
          memset(deviceMACs, 0, sizeof(deviceMACs));
          WiFi.scanDelete(); 
          
          needsRedraw = true;
          rssiLastTick = millis();
          WiFi.scanNetworks(true, false, false, 100, targetChannel);
        }
      }
    } 
    else if (wifiView == 1) {
      // RSSI ➡️ Traffic
      wifiView = 2;
      packetCount = 0;
      deviceCount = 0;
      memset(deviceMACs, 0, sizeof(deviceMACs));
      esp_wifi_set_promiscuous_rx_cb(&sniffer_callback);
      esp_wifi_set_promiscuous(true);
      esp_wifi_set_channel(targetChannel, WIFI_SECOND_CHAN_NONE);
      wifiLastTick = millis(); 
      needsRedraw = true;
    } 
    else if (wifiView == 2) {
      // Traffic ➡️ Scanner
      esp_wifi_set_promiscuous(false);
      wifiView = 0;
      needsRedraw = true;
      lastScanStatus = -99;
      WiFi.scanNetworks(true);
    }
  }
  else if (ev == EV_LEFT_TAP) {
    buzzNote(800, 200);
    if (wifiView == 1 || wifiView == 2) {
      if (wifiView == 2) esp_wifi_set_promiscuous(false);
      wifiView = 0; // Go back to scanner
      needsRedraw = true;
      lastScanStatus = -99;
      WiFi.scanNetworks(true);
    } else {
      AppManager_ReturnToMenu();
    }
  }
}
