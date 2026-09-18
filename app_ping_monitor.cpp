// ─────────────────────────────────────────────────────────────────────────────
//  app_ping_monitor.cpp  —  Ping Monitor Application
//  ESP32 Dual-OLED Macro Keyboard  |  Layer 9
//
//  Architecture
//  ────────────
//  • Non-blocking millis()-driven state machine.
//  • One blocking ICMP ping is isolated inside a short-lived FreeRTOS task
//    (same pattern as the project's existing beepTask).  The UI loop never
//    waits for the ping task.
//  • BLE/Wi-Fi radio handoff mirrors the existing WiFi Analyzer (layer 2).
//  • All display output uses the shared oled / oled2 objects from globals.h.
//  • All input comes through AppPingMonitor_HandleEvent(LogicalEvent).
//
//  REQUIRES: "ESPping" library by dvarrel
//    arduino-cli lib install --zip-path ESPping.zip
//    (installed from: https://github.com/dvarrel/ESPping )
// ─────────────────────────────────────────────────────────────────────────────

#include "app_ping_monitor.h"
#include <WiFi.h>
#include <ESPping.h>  // ESPping library by dvarrel — installed as ESPping

// Wi-Fi credential accessors — implemented in Macro-Keyboard.ino
extern int         GetWifiNetCount();
extern const char* GetWifiSSID(int index);
extern const char* GetWifiPass(int index);

// ── Configuration ─────────────────────────────────────────────────────────────

// PUBG Mobile: leave empty ("") — no verified ICMP-responsive endpoint exists.
// If you confirm a pingable IP, set it here.
#define PING_PUBG_HOST    ""

// Custom target: set to your desired IP or hostname, or leave empty.
#define PING_CUSTOM_HOST  ""

#define PING_TIMEOUT_MS      1000   // per-ping ICMP timeout
#define CONNECT_TIMEOUT_MS   15000  // initial WiFi connect timeout
#define RECONNECT_TIMEOUT_MS 20000  // reconnect timeout after drop
#define PING_HISTORY_SIZE    90     // rolling sample buffer depth

// OLED1 graph area geometry
#define GX0  4      // left edge (X axis tick)
#define GY0  10     // top  (low latency)
#define GY1  50     // bottom (high latency) — 40 px tall
#define GH   (GY1 - GY0)        // 40
#define GW   (128  - GX0)       // 124

// History slot sentinels
#define HIST_EMPTY   ((uint16_t)0x0000)
#define HIST_TIMEOUT ((uint16_t)0xFFFF)
#define HIST_UNAVAIL ((uint16_t)0xFFFE)

// pingResultMs special codes
#define PING_RES_PENDING  (-2)
#define PING_RES_TIMEOUT  (-1)
#define PING_RES_UNAVAIL  (-3)

// Draw rate cap for monitoring screens
#define DRAW_PERIOD_MS  100   // max 10 FPS

// ── State Machine ─────────────────────────────────────────────────────────────
enum PingMonState : uint8_t {
    PMST_WIFI_SELECT = 0,
    PMST_CONNECTING,
    PMST_TARGET_SELECT,
    PMST_MONITORING,
    PMST_PAUSED,
    PMST_RECONNECTING,
    PMST_RESET_CONFIRM,
    PMST_ERROR,
};

// ── Ping Targets ──────────────────────────────────────────────────────────────
struct PingTargetDef {
    const char* name;        // short display label
    const char* host;        // hostname/IP — nullptr = dynamic gateway
    bool        gatewayMode; // true → resolve from WiFi.gatewayIP()
};

static const PingTargetDef TARGETS[] = {
    { "ROUTER",     nullptr,          true  }, // 0: dynamic
    { "GOOGLE",     "google.com",     false }, // 1
    { "CLOUDFLARE", "1.1.1.1",       false }, // 2
    { "PUBG MOBILE",PING_PUBG_HOST,   false }, // 3: configurable, see #define above
    { "CUSTOM",     PING_CUSTOM_HOST, false }, // 4: configurable, see #define above
};
static const int TARGET_COUNT = 5;

// Configurable ping intervals
static const uint32_t PING_IVLS[]  = { 1000, 2000, 5000, 10000 };
static const int       PING_IVL_N  = 4;

// ── Internal State ─────────────────────────────────────────────────────────────
static PingMonState   pmState        = PMST_WIFI_SELECT;
static int            wifiSel        = 0;
static int            targetSel      = 0;
static char           gatewayStr[20] = {};   // "x.x.x.x\0"
static char           currentHost[64]= {};   // host being pinged this session
static bool           hostUnavail    = false;

static unsigned long  connectStart   = 0;
static int            ivlIdx         = 0;    // ping interval index
#define CUR_IVL()     PING_IVLS[ivlIdx]

static unsigned long  lastPingDisp   = 0;    // when last ping was dispatched
static unsigned long  lastDrawMs     = 0;
static bool           needsRedraw    = true;
static int            resetConfSel   = 0;    // 0=NO 1=YES
static char           errorMsg[24]   = {};

// ── Ping Task ─────────────────────────────────────────────────────────────────
// Pattern: identical to the project's beepTask.
// One task per ping; task self-deletes; main loop reads result after task ends.

static volatile bool    pingRunning   = false;
static volatile int32_t pingResultMs  = PING_RES_PENDING;
static TaskHandle_t     pingHandle    = nullptr;
static char             pingHostBuf[64];

static void pmPingTask(void* pv) {
    // Runs on Core 0 — performs one blocking ICMP ping, stores result, terminates.
    bool ok = Ping.ping(pingHostBuf, 1);
    if (ok) {
        float ms = Ping.averageTime();
        pingResultMs = (ms > 0.0f) ? (int32_t)constrain((long)ms, 1L, 65000L) : 1;
    } else {
        pingResultMs = PING_RES_TIMEOUT;
    }
    pingRunning = false;
    pingHandle  = nullptr;
    vTaskDelete(NULL);
}

// Dispatch one ping.  No-op if a task is already running.
static void dispatchPing() {
    if (pingRunning) return;

    if (hostUnavail || currentHost[0] == '\0') {
        pingResultMs = PING_RES_UNAVAIL;
        return;
    }

    strncpy(pingHostBuf, currentHost, sizeof(pingHostBuf) - 1);
    pingHostBuf[sizeof(pingHostBuf) - 1] = '\0';

    pingRunning  = true;
    pingResultMs = PING_RES_PENDING;
    xTaskCreatePinnedToCore(pmPingTask, "pmPing", 4096,
                             nullptr, 1, &pingHandle, 0);
}

// Kill any running ping task (called on exit / WiFi loss).
static void killPingTask() {
    if (pingHandle != nullptr) {
        vTaskDelete(pingHandle);
        pingHandle = nullptr;
    }
    pingRunning  = false;
    pingResultMs = PING_RES_PENDING;
}

// ── Rolling History ───────────────────────────────────────────────────────────
static uint16_t hist[PING_HISTORY_SIZE];
static int      histHead  = 0;
static int      histCount = 0;

// ── Statistics ────────────────────────────────────────────────────────────────
static uint32_t totSent   = 0;
static uint32_t totRecv   = 0;
static uint32_t minPing   = 0xFFFFFFFFu;
static uint32_t maxPing   = 0;
static uint64_t sumPing   = 0;
static int32_t  prevOkMs  = -1;   // last successful ms (jitter base)
static uint32_t jitterMs  = 0;

// Current display values (updated when result arrives)
static uint32_t curMs      = 0;
static bool     curTimeout = false;
static bool     curUnavail = false;

static void resetStats() {
    for (int i = 0; i < PING_HISTORY_SIZE; i++) hist[i] = HIST_EMPTY;
    histHead = 0; histCount = 0;
    totSent = 0; totRecv = 0;
    minPing = 0xFFFFFFFFu; maxPing = 0; sumPing = 0;
    prevOkMs = -1; jitterMs = 0;
    curMs = 0; curTimeout = false; curUnavail = false;
}

static void recordResult(int32_t res) {
    totSent++;
    uint16_t slot;

    if (res == PING_RES_UNAVAIL) {
        slot = HIST_UNAVAIL;
        curUnavail = true;  curTimeout = false;
    } else if (res < 0) {
        slot = HIST_TIMEOUT;
        curTimeout = true;  curUnavail = false;
    } else {
        uint32_t ms = (uint32_t)res;
        slot = (ms > 65000u) ? (uint16_t)65000u : (uint16_t)ms;

        totRecv++;
        curMs      = ms;
        curTimeout = false;
        curUnavail = false;

        if (ms < minPing) minPing = ms;
        if (ms > maxPing) maxPing = ms;
        sumPing += ms;

        // Jitter: EWMA of |current - previous| successful pings
        if (prevOkMs >= 0) {
            uint32_t diff = (ms > (uint32_t)prevOkMs) ?
                            (ms - (uint32_t)prevOkMs) :
                            ((uint32_t)prevOkMs - ms);
            jitterMs = (jitterMs * 3u + diff) / 4u; // α = 0.25
        }
        prevOkMs = (int32_t)ms;
    }

    hist[histHead] = slot;
    histHead = (histHead + 1) % PING_HISTORY_SIZE;
    if (histCount < PING_HISTORY_SIZE) histCount++;
    pingResultMs = PING_RES_PENDING; // mark as consumed here too, safety
}

// ── Quality Label ─────────────────────────────────────────────────────────────
static const char* qualityStr() {
    if (curUnavail)      return "  N/A  ";
    if (curTimeout)      return "TIMEOUT";
    if (curMs <  30)     return "EXCLLNT";
    if (curMs <  60)     return " GOOD  ";
    if (curMs < 100)     return " FAIR  ";
    if (curMs < 200)     return " HIGH  ";
    return                      "V.HIGH ";
}

// ── Resolve current ping host ─────────────────────────────────────────────────
static void resolveHost() {
    const PingTargetDef& t = TARGETS[targetSel];
    if (t.gatewayMode) {
        strncpy(currentHost, gatewayStr, sizeof(currentHost) - 1);
        currentHost[sizeof(currentHost) - 1] = '\0';
        hostUnavail = (strlen(currentHost) < 7); // "x.x.x.x" min 7 chars
    } else if (!t.host || t.host[0] == '\0') {
        currentHost[0] = '\0';
        hostUnavail = true;
    } else {
        strncpy(currentHost, t.host, sizeof(currentHost) - 1);
        currentHost[sizeof(currentHost) - 1] = '\0';
        hostUnavail = false;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  DRAWING HELPERS
// ─────────────────────────────────────────────────────────────────────────────

// Graph on OLED1 (y=10..50, x=4..127)
static void drawGraph() {
    // Axes
    oled.drawFastVLine(GX0 - 1, GY0, GH + 1, SSD1306_WHITE);
    oled.drawFastHLine(GX0 - 1, GY1, GW + 1, SSD1306_WHITE);

    if (histCount == 0) {
        oled.setTextSize(1);
        oled.setCursor(10, 26);
        oled.print("Waiting for data...");
        return;
    }

    // Auto-scale: select tier from observed max
    uint32_t maxVal = 1;
    for (int i = 0; i < histCount; i++) {
        int idx = (histHead - histCount + i + PING_HISTORY_SIZE) % PING_HISTORY_SIZE;
        uint16_t v = hist[idx];
        if (v != HIST_EMPTY && v != HIST_TIMEOUT && v != HIST_UNAVAIL && v > maxVal)
            maxVal = v;
    }
    uint32_t scale;
    if      (maxVal <=  50) scale =   50;
    else if (maxVal <= 100) scale =  100;
    else if (maxVal <= 200) scale =  200;
    else if (maxVal <= 500) scale =  500;
    else                    scale = 1000;

    // Y top label
    oled.setTextSize(1);
    char lbl[6]; snprintf(lbl, 6, "%u", (unsigned)scale);
    oled.setCursor(0, GY0);
    oled.print(lbl);

    // Plot samples — newest on right, oldest on left
    int show  = min(histCount, GW);
    int prevX = -1, prevY = -1;

    for (int i = 0; i < show; i++) {
        int off = histCount - show + i;
        int idx = (histHead - histCount + off + PING_HISTORY_SIZE) % PING_HISTORY_SIZE;
        uint16_t v = hist[idx];

        // Right-align: fill toward the right as samples accumulate
        int px = GX0 + (GW - show) + i;

        if (v == HIST_EMPTY) {
            prevX = -1; prevY = -1;
            continue;
        }
        if (v == HIST_TIMEOUT || v == HIST_UNAVAIL) {
            // Tick at baseline marks a failed/unavailable sample
            oled.drawFastVLine(px, GY1 - 4, 4, SSD1306_WHITE);
            prevX = -1; prevY = -1;
            continue;
        }

        // Valid: map to pixel row
        long mapped = map((long)constrain((long)v, 0L, (long)scale),
                          0L, (long)scale, 0L, (long)GH);
        int py = (int)(GY1 - mapped);
        py = constrain(py, GY0, GY1);

        if (prevX >= 0) oled.drawLine(prevX, prevY, px, py, SSD1306_WHITE);
        else            oled.drawPixel(px, py, SSD1306_WHITE);

        prevX = px; prevY = py;
    }

    // Bottom scale label
    char bot[12]; snprintf(bot, 12, "0-%ums", (unsigned)scale);
    oled.setTextSize(1);
    oled.setCursor(GX0, GY1 + 2);
    oled.print(bot);
}

// Quality bar at y=57..63 on OLED1
static void drawQualityBar() {
    const int BX = 0, BY = 57, BW = 80, BH = 6;
    int fillW = 0;
    if (!curTimeout && !curUnavail && curMs > 0) {
        if      (curMs <  30) fillW = BW - 2;
        else if (curMs <  60) fillW = (BW - 2) * 75 / 100;
        else if (curMs < 100) fillW = (BW - 2) * 50 / 100;
        else if (curMs < 200) fillW = (BW - 2) * 25 / 100;
        else                  fillW = (BW - 2) *  8 / 100;
    }
    oled.drawRect(BX, BY, BW, BH, SSD1306_WHITE);
    if (fillW > 0) oled.fillRect(BX + 1, BY + 1, fillW, BH - 2, SSD1306_WHITE);
    oled.setTextSize(1);
    oled.setCursor(BX + BW + 3, BY);
    oled.print(qualityStr());
}

// Stats block on OLED2 (call after clearDisplay + d2Header)
static void drawStatsBlock() {
    char buf[9], b[6];
    oled2.setTextSize(1);

    // Row 1: NOW | MIN
    if      (curUnavail)  d2L(16, "NOW N/A ");
    else if (curTimeout)  d2L(16, "NOW TOUT");
    else if (curMs == 0)  d2L(16, "NOW  ---");
    else {
        uint32_t display_ms = (curMs > 99999u) ? 99999u : curMs;
        snprintf(buf, 9, "NOW%5lu", (unsigned long)display_ms); d2L(16, buf);
    }

    if (minPing == 0xFFFFFFFFu) { d2R(16, "MN---"); }
    else {
        uint32_t mn = (minPing > 999u) ? 999u : minPing;
        snprintf(b, 6, "MN%3lu", (unsigned long)mn); d2R(16, b);
    }

    // Row 2: AVG | MAX
    {
        uint32_t avg = (totRecv > 0) ? (uint32_t)((uint64_t)sumPing / totRecv) : 0;
        if (avg > 99999u) avg = 99999u;
        snprintf(buf, 9, "AVG%5lu", (unsigned long)avg); d2L(25, buf);
    }
    if (totRecv == 0) { d2R(25, "MX---"); }
    else {
        uint32_t mx = (maxPing > 999u) ? 999u : maxPing;
        snprintf(b, 6, "MX%3lu", (unsigned long)mx); d2R(25, b);
    }

    // Row 3: JIT | LOSS
    {
        uint32_t jit = (jitterMs > 99999u) ? 99999u : jitterMs;
        snprintf(buf, 9, "JIT%5lu", (unsigned long)jit); d2L(34, buf);
    }
    {
        // Guard: totRecv can never exceed totSent, but saturate at 100% to be safe
        uint32_t failed = (totSent >= totRecv) ? (totSent - totRecv) : 0;
        uint8_t  loss   = (totSent > 0) ? (uint8_t)((failed * 100u) / totSent) : 0;
        if (loss > 100) loss = 100;
        snprintf(b, 6, "L%3u%%", (unsigned)loss); d2R(34, b);
    }

    // Row 4: Packets
    uint32_t dRecv = (totRecv > 9999u) ? 9999u : totRecv;
    uint32_t dSent = (totSent > 9999u) ? 9999u : totSent;
    snprintf(buf, 9, "%lu/%lu",
             (unsigned long)dRecv,
             (unsigned long)dSent);
    d2L(43, buf);
    d2R(43, "PKTS");
}

// ─────────────────────────────────────────────────────────────────────────────
//  SCREEN DRAW FUNCTIONS
// ─────────────────────────────────────────────────────────────────────────────

static void screenWifiSelect() {
    int n = GetWifiNetCount();
    // ── OLED1 ──
    oled.clearDisplay();
    oled.setTextColor(SSD1306_WHITE);
    oled.fillRect(0, 0, 128, 10, SSD1306_WHITE);
    oled.setTextColor(SSD1306_BLACK);
    oled.setTextSize(1);
    oled.setCursor(2, 2); oled.print("SELECT WIFI");
    oled.setTextColor(SSD1306_WHITE);

    int start = max(0, wifiSel - 2);
    if (start + 5 > n) start = max(0, n - 5);
    for (int i = 0; i < 5 && start + i < n; i++) {
        int idx = start + i;
        int y   = 13 + i * 10;
        if (idx == wifiSel) {
            oled.fillRect(0, y - 1, 128, 10, SSD1306_WHITE);
            oled.setTextColor(SSD1306_BLACK);
        } else {
            oled.setTextColor(SSD1306_WHITE);
        }
        oled.setCursor(2, y);
        oled.print((idx == wifiSel) ? "> " : "  ");
        char ssid[19]; strncpy(ssid, GetWifiSSID(idx), 18); ssid[18] = '\0';
        oled.print(ssid);
    }
    oled.setTextColor(SSD1306_WHITE);
    oled.setCursor(0, 57); oled.print("CTR=OK  L=Back");
    oled.display();

    // ── OLED2 ──
    oled2.clearDisplay(); oled2.setTextColor(SSD1306_WHITE); oled2.setTextSize(1);
    d2Header("WIFI", "SEL"); d2Divider();
    d2L(18, "CHOOSE"); d2L(28, "WI-FI"); d2L(38, "NETWORK");
    d2R(28, "CTR"); d2R(38, "=OK");
    oled2.display();
}

static void screenConnecting(unsigned long timeout, const char* title) {
    unsigned long elapsed = millis() - connectStart;
    int pct = (int)constrain((long)elapsed * 100L / (long)timeout, 0L, 99L);

    // ── OLED1 ──
    oled.clearDisplay(); oled.setTextColor(SSD1306_WHITE); oled.setTextSize(1);
    centered(title, 4);
    char ssid[22]; strncpy(ssid, GetWifiSSID(wifiSel), 21); ssid[21] = '\0';
    centered(ssid, 18);
    oled.drawRect(10, 32, 108, 8, SSD1306_WHITE);
    int fw = (int)((long)106 * pct / 100);
    if (fw > 0) oled.fillRect(11, 33, fw, 6, SSD1306_WHITE);
    wl_status_t ws = WiFi.status();
    const char* st = "Connecting...";
    if (ws == WL_NO_SSID_AVAIL)  st = "Network not found";
    if (ws == WL_CONNECT_FAILED) st = "Auth failed";
    centered(st, 46);
    oled.display();

    // ── OLED2 ──
    oled2.clearDisplay(); oled2.setTextColor(SSD1306_WHITE); oled2.setTextSize(1);
    d2Header("WIFI", "..."); d2Divider();
    d2L(20, ssid);
    char pb[8]; snprintf(pb, 8, "%d%%", pct); d2L(32, pb);
    if ((millis() / 300) % 2 == 0) d2R(26, "....");
    oled2.display();
}

static void screenTargetSelect() {
    // ── OLED1 ──
    oled.clearDisplay(); oled.setTextColor(SSD1306_WHITE);
    oled.fillRect(0, 0, 128, 10, SSD1306_WHITE);
    oled.setTextColor(SSD1306_BLACK); oled.setTextSize(1);
    oled.setCursor(2, 2); oled.print("SELECT TARGET");
    oled.setTextColor(SSD1306_WHITE);
    for (int i = 0; i < TARGET_COUNT; i++) {
        int y = 13 + i * 10;
        if (i == targetSel) {
            oled.fillRect(0, y - 1, 128, 10, SSD1306_WHITE);
            oled.setTextColor(SSD1306_BLACK);
        } else {
            oled.setTextColor(SSD1306_WHITE);
        }
        oled.setCursor(2, y);
        oled.print((i == targetSel) ? "> " : "  ");
        oled.print(TARGETS[i].name);
    }
    oled.setTextColor(SSD1306_WHITE);
    oled.setCursor(0, 57); oled.print("CTR=OK  L=Back");
    oled.display();

    // ── OLED2: target details ──
    oled2.clearDisplay(); oled2.setTextColor(SSD1306_WHITE); oled2.setTextSize(1);
    d2Header("TARGET", "SEL"); d2Divider();
    d2L(18, TARGETS[targetSel].name);
    if (TARGETS[targetSel].gatewayMode) {
        d2L(30, gatewayStr[0] ? gatewayStr : "N/A");
        d2L(40, "(Gateway)");
    } else if (!TARGETS[targetSel].host || TARGETS[targetSel].host[0] == '\0') {
        d2L(30, "N/A");
        d2L(40, "UNAVAIL");
    } else {
        d2L(30, TARGETS[targetSel].host);
    }
    oled2.display();
}

static void screenMonitoring() {
    // ── OLED1: graph + header + quality bar ──
    oled.clearDisplay(); oled.setTextColor(SSD1306_WHITE);

    // Inverted header bar
    oled.fillRect(0, 0, 128, 9, SSD1306_WHITE);
    oled.setTextColor(SSD1306_BLACK); oled.setTextSize(1);
    char nameH[13]; strncpy(nameH, TARGETS[targetSel].name, 12); nameH[12] = '\0';
    oled.setCursor(2, 1); oled.print(nameH);

    // Current ping (right-aligned)
    char pingH[10];
    if      (curUnavail)  strncpy(pingH, "N/A", 9);
    else if (curTimeout)  strncpy(pingH, "TOUT", 9);
    else if (curMs == 0)  strncpy(pingH, "---", 9);
    else {
        uint32_t dms = (curMs > 9999u) ? 9999u : curMs;
        snprintf(pingH, 9, "%lums", (unsigned long)dms);
    }
    pingH[8] = '\0';
    oled.setCursor(126 - (int)strlen(pingH) * 6, 1);
    oled.print(pingH);
    oled.setTextColor(SSD1306_WHITE);

    drawGraph();
    drawQualityBar();
    oled.display();

    // ── OLED2: live statistics ──
    oled2.clearDisplay(); oled2.setTextColor(SSD1306_WHITE); oled2.setTextSize(1);
    d2Header("PING", "STAT"); d2Divider();
    drawStatsBlock();
    oled2.display();
}

static void screenPaused() {
    // ── OLED1: frozen graph + pause overlay ──
    oled.clearDisplay(); oled.setTextColor(SSD1306_WHITE);
    oled.fillRect(0, 0, 128, 9, SSD1306_WHITE);
    oled.setTextColor(SSD1306_BLACK); oled.setTextSize(1);
    oled.setCursor(26, 1); oled.print("** PAUSED **");
    oled.setTextColor(SSD1306_WHITE);
    drawGraph();
    drawQualityBar();
    // Pause hint box
    oled.fillRect(16, 26, 96, 22, SSD1306_BLACK);
    oled.drawRect(16, 26, 96, 22, SSD1306_WHITE);
    oled.setCursor(22, 29); oled.print("CTR = Resume");
    oled.setCursor(22, 39); oled.print("L   = Exit");
    oled.display();

    // ── OLED2: stats (frozen) ──
    oled2.clearDisplay(); oled2.setTextColor(SSD1306_WHITE); oled2.setTextSize(1);
    d2Header("PAUSED", "STOP"); d2Divider();
    drawStatsBlock();
    oled2.display();
}

static void screenResetConfirm() {
    oled.clearDisplay(); oled.setTextColor(SSD1306_WHITE); oled.setTextSize(1);
    centered("RESET STATS?", 6);
    oled.drawLine(10, 16, 118, 16, SSD1306_WHITE);

    // NO
    if (resetConfSel == 0) { oled.fillRect(8,  24, 52, 16, SSD1306_WHITE); oled.setTextColor(SSD1306_BLACK); }
    else                   { oled.drawRect(8,  24, 52, 16, SSD1306_WHITE); oled.setTextColor(SSD1306_WHITE); }
    oled.setCursor(24, 28); oled.print("NO");
    oled.setTextColor(SSD1306_WHITE);

    // YES
    if (resetConfSel == 1) { oled.fillRect(68, 24, 52, 16, SSD1306_WHITE); oled.setTextColor(SSD1306_BLACK); }
    else                   { oled.drawRect(68, 24, 52, 16, SSD1306_WHITE); oled.setTextColor(SSD1306_WHITE); }
    oled.setCursor(82, 28); oled.print("YES");
    oled.setTextColor(SSD1306_WHITE);

    oled.setCursor(4, 46); oled.print("UP/DN=Select");
    oled.setCursor(4, 54); oled.print("CTR=OK  L=Cancel");
    oled.display();

    oled2.clearDisplay(); oled2.setTextColor(SSD1306_WHITE); oled2.setTextSize(1);
    d2Header("RESET", "STS");
    d2L(22, "CLEAR ALL"); d2L(34, "STATS?");
    oled2.display();
}

static void screenError() {
    oled.clearDisplay(); oled.setTextColor(SSD1306_WHITE);
    oled.fillRect(0, 0, 128, 10, SSD1306_WHITE);
    oled.setTextColor(SSD1306_BLACK); oled.setTextSize(1);
    oled.setCursor(2, 2); oled.print("CONNECTION FAILED");
    oled.setTextColor(SSD1306_WHITE);
    centered(errorMsg, 22);
    centered("CTR=Retry", 38);
    centered("L=Select WiFi", 50);
    oled.display();

    oled2.clearDisplay(); oled2.setTextColor(SSD1306_WHITE); oled2.setTextSize(1);
    d2Header("ERROR", "!ERR"); d2Divider();
    d2L(22, errorMsg);
    d2L(34, "CTR=Retry");
    d2L(44, "L=WiFiSel");
    oled2.display();
}

// ─────────────────────────────────────────────────────────────────────────────
//  STATE UPDATE FUNCTIONS  (called once per loop iteration)
// ─────────────────────────────────────────────────────────────────────────────

static void updWifiSelect() {
    if (needsRedraw) { screenWifiSelect(); needsRedraw = false; }
}

static void updConnecting() {
    if (millis() - lastDrawMs >= 200) {
        lastDrawMs = millis();
        screenConnecting(CONNECT_TIMEOUT_MS, "CONNECTING...");
    }
    wl_status_t ws = WiFi.status();
    if (ws == WL_CONNECTED) {
        IPAddress gw = WiFi.gatewayIP();
        snprintf(gatewayStr, sizeof(gatewayStr), "%d.%d.%d.%d",
                 gw[0], gw[1], gw[2], gw[3]);
        pmState = PMST_TARGET_SELECT; needsRedraw = true;
    } else if (millis() - connectStart >= CONNECT_TIMEOUT_MS) {
        strncpy(errorMsg, "TIMED OUT", sizeof(errorMsg) - 1);
        pmState = PMST_ERROR; needsRedraw = true;
    } else if (ws == WL_CONNECT_FAILED || ws == WL_NO_SSID_AVAIL) {
        strncpy(errorMsg, "CONN FAILED", sizeof(errorMsg) - 1);
        pmState = PMST_ERROR; needsRedraw = true;
    }
}

static void updTargetSelect() {
    if (needsRedraw) { screenTargetSelect(); needsRedraw = false; }
}

static void updMonitoring() {
    unsigned long now = millis();

    // Wi-Fi watchdog
    if (WiFi.status() != WL_CONNECTED) {
        killPingTask();
        WiFi.disconnect(false);
        delay(30);
        WiFi.begin(GetWifiSSID(wifiSel), GetWifiPass(wifiSel));
        connectStart = now;
        pmState      = PMST_RECONNECTING;
        needsRedraw  = true;
        return;
    }

    // Consume finished ping result
    if (!pingRunning && pingResultMs != PING_RES_PENDING) {
        recordResult(pingResultMs);
        pingResultMs = PING_RES_PENDING; // consumed
    }

    // Schedule next ping
    if (!pingRunning && (now - lastPingDisp >= CUR_IVL())) {
        lastPingDisp = now;
        dispatchPing();
    }

    // Rate-limited redraw
    if (now - lastDrawMs >= DRAW_PERIOD_MS) {
        lastDrawMs = now;
        screenMonitoring();
    }
}

static void updPaused() {
    // Refresh paused display at 2 FPS (no new pings)
    if (millis() - lastDrawMs >= 500) {
        lastDrawMs = millis();
        screenPaused();
    }
}

static void updReconnecting() {
    if (millis() - lastDrawMs >= 200) {
        lastDrawMs = millis();
        screenConnecting(RECONNECT_TIMEOUT_MS, "RECONNECTING...");
    }
    wl_status_t ws = WiFi.status();
    if (ws == WL_CONNECTED) {
        IPAddress gw = WiFi.gatewayIP();
        snprintf(gatewayStr, sizeof(gatewayStr), "%d.%d.%d.%d",
                 gw[0], gw[1], gw[2], gw[3]);
        resolveHost();
        lastPingDisp = 0; // trigger immediate ping
        pmState      = PMST_MONITORING;
        lastDrawMs   = 0;
    } else if (millis() - connectStart >= RECONNECT_TIMEOUT_MS) {
        strncpy(errorMsg, "WIFI LOST", sizeof(errorMsg) - 1);
        pmState = PMST_ERROR; needsRedraw = true;
    }
}

static void updResetConfirm() {
    if (needsRedraw) { screenResetConfirm(); needsRedraw = false; }
}

static void updError() {
    if (needsRedraw) { screenError(); needsRedraw = false; }
}

// ─────────────────────────────────────────────────────────────────────────────
//  PUBLIC API
// ─────────────────────────────────────────────────────────────────────────────

void AppPingMonitor_Init() {
    // ── Radios are now managed by AppManager container system ──
    WiFi.disconnect();
    delay(10);

    // ── Reset all internal state ──
    pmState       = PMST_WIFI_SELECT;
    wifiSel       = 0;
    targetSel     = 0;
    gatewayStr[0] = '\0';
    currentHost[0]= '\0';
    hostUnavail   = false;
    ivlIdx        = 0;
    lastPingDisp  = 0;
    lastDrawMs    = 0;
    needsRedraw   = true;
    resetConfSel  = 0;
    errorMsg[0]   = '\0';
    pingRunning   = false;
    pingResultMs  = PING_RES_PENDING;
    pingHandle    = nullptr;

    resetStats();
}

void AppPingMonitor_Update() {
    switch (pmState) {
        case PMST_WIFI_SELECT:   updWifiSelect();    break;
        case PMST_CONNECTING:    updConnecting();    break;
        case PMST_TARGET_SELECT: updTargetSelect();  break;
        case PMST_MONITORING:    updMonitoring();    break;
        case PMST_PAUSED:        updPaused();        break;
        case PMST_RECONNECTING:  updReconnecting();  break;
        case PMST_RESET_CONFIRM: updResetConfirm();  break;
        case PMST_ERROR:         updError();         break;
    }
}

void AppPingMonitor_HandleEvent(LogicalEvent ev) {
    resetIdle();

    // Universal exit: matrix * held 2 s
    if (ev == EV_BACKSPACE_HOLD_2S) {
        AppManager_ReturnToMenu();
        return;
    }

    switch (pmState) {

        // ── WiFi select ──────────────────────────────────────────────────────
        case PMST_WIFI_SELECT:
            if (ev == EV_UP_TAP || ev == EV_UP_HOLD) {
                if (wifiSel > 0) { wifiSel--; needsRedraw = true; }
            } else if (ev == EV_DOWN_TAP || ev == EV_DOWN_HOLD) {
                if (wifiSel < GetWifiNetCount() - 1) { wifiSel++; needsRedraw = true; }
            } else if (ev == EV_CENTER_TAP) {
                WiFi.begin(GetWifiSSID(wifiSel), GetWifiPass(wifiSel));
                connectStart = millis();
                lastDrawMs   = 0;
                pmState      = PMST_CONNECTING;
            } else if (ev == EV_LEFT_TAP) {
                AppManager_ReturnToMenu(); // exit app
            }
            break;

        // ── Connecting ───────────────────────────────────────────────────────
        case PMST_CONNECTING:
            if (ev == EV_LEFT_TAP) {
                WiFi.disconnect(true);
                pmState     = PMST_WIFI_SELECT;
                needsRedraw = true;
            }
            break;

        // ── Target select ────────────────────────────────────────────────────
        case PMST_TARGET_SELECT:
            if (ev == EV_UP_TAP || ev == EV_UP_HOLD) {
                if (targetSel > 0) { targetSel--; needsRedraw = true; }
            } else if (ev == EV_DOWN_TAP || ev == EV_DOWN_HOLD) {
                if (targetSel < TARGET_COUNT - 1) { targetSel++; needsRedraw = true; }
            } else if (ev == EV_CENTER_TAP) {
                resolveHost();
                resetStats();
                lastPingDisp = 0;
                lastDrawMs   = 0;
                pmState      = PMST_MONITORING;
            } else if (ev == EV_LEFT_TAP) {
                // Back to WiFi select — disconnect first
                WiFi.disconnect(true);
                delay(50);
                pmState     = PMST_WIFI_SELECT;
                needsRedraw = true;
            }
            break;

        // ── Monitoring ───────────────────────────────────────────────────────
        case PMST_MONITORING:
            if (ev == EV_CENTER_TAP) {
                pmState = PMST_PAUSED; needsRedraw = true;
            } else if (ev == EV_LEFT_TAP) {
                AppManager_ReturnToMenu();
            } else if (ev == EV_UP_TAP) {
                // Cycle ping interval
                ivlIdx = (ivlIdx + 1) % PING_IVL_N;
            } else if (ev == EV_DOWN_TAP) {
                // Open reset confirm
                resetConfSel = 0;
                pmState      = PMST_RESET_CONFIRM;
                needsRedraw  = true;
            }
            break;

        // ── Paused ───────────────────────────────────────────────────────────
        case PMST_PAUSED:
            if (ev == EV_CENTER_TAP) {
                pmState    = PMST_MONITORING;
                lastDrawMs = 0;
            } else if (ev == EV_LEFT_TAP) {
                AppManager_ReturnToMenu();
            }
            break;

        // ── Reconnecting ─────────────────────────────────────────────────────
        case PMST_RECONNECTING:
            if (ev == EV_LEFT_TAP) {
                WiFi.disconnect(true);
                pmState     = PMST_WIFI_SELECT;
                needsRedraw = true;
            }
            break;

        // ── Reset confirm ────────────────────────────────────────────────────
        case PMST_RESET_CONFIRM:
            if (ev == EV_UP_TAP || ev == EV_DOWN_TAP ||
                ev == EV_LEFT_TAP || ev == EV_RIGHT_TAP) {
                resetConfSel = 1 - resetConfSel;
                needsRedraw  = true;
            } else if (ev == EV_CENTER_TAP) {
                if (resetConfSel == 1) resetStats();
                pmState    = PMST_MONITORING;
                lastDrawMs = 0;
            }
            break;

        // ── Error ────────────────────────────────────────────────────────────
        case PMST_ERROR:
            if (ev == EV_CENTER_TAP) {
                // Retry connection
                WiFi.begin(GetWifiSSID(wifiSel), GetWifiPass(wifiSel));
                connectStart = millis();
                lastDrawMs   = 0;
                pmState      = PMST_CONNECTING;
            } else if (ev == EV_LEFT_TAP) {
                WiFi.disconnect(true);
                pmState     = PMST_WIFI_SELECT;
                needsRedraw = true;
            }
            break;

        default: break;
    }
}

void AppPingMonitor_Exit() {
    // ── Kill any in-flight ping task ──
    killPingTask();

    // ── Radios are now managed by AppManager container system ──
    WiFi.disconnect();
    delay(10);

    // ── Reset state for clean re-entry ──
    pmState      = PMST_WIFI_SELECT;
    needsRedraw  = true;
    pingResultMs = PING_RES_PENDING;
}
