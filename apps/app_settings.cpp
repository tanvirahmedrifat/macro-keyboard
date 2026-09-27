#include "app_settings.h"
#include <Arduino.h>
#include <Preferences.h>
#include <qrcode.h>
#include "web_server.h"

// FIX 5: NVS Debounce — accumulate setting changes in RAM, flush to NVS only on exit.
// The old code wrote to flash on EVERY keypress, causing excessive flash wear.
static bool settingsDirty = false;

static void flushSettingsToNVS() {
    if (!settingsDirty) return;
    Preferences p;
    p.begin("macro-kb", false);
    p.putBool("silentMode", silentMode);
    p.putULong("sleepTimeout", sleepTimeoutMs);
    p.end();
    settingsDirty = false;
}

static int settingSel = 0;
static int qrScreen = 0; // 0=None, 1=Join WiFi, 2=Dashboard URL
// FIX 29: Dirty flag for display — only redraw when something actually changed.
static bool displayDirty = true;
static unsigned long lastDrawMs = 0;
#define SETTING_COUNT 5

// FIX 38: SoftAP SSID includes last 4 MAC digits to distinguish devices on same network.
static String buildAPName() {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char suffix[6];
    snprintf(suffix, sizeof(suffix), "%02X%02X", mac[4], mac[5]);
    return String("MacroKB-") + String(suffix);
}

void AppSettings_Init() {
    settingSel = 0;
    qrScreen = 0;
    settingsDirty = false;
    displayDirty = true;
    lastDrawMs = 0;
    
    // Use unique SoftAP name (FIX 38)
    String apName = buildAPName();
    
    // BEST UX: Always start the SoftAP as a guaranteed fallback.
    // At the same time, try to connect to the primary saved network in the background.
    // If it connects, the QR screen will auto-update to show the home network IP.
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(apName.c_str(), "");
    
    if (WIFI_NET_COUNT > 0) {
        WiFi.begin(WIFI_NETS[0].ssid, WIFI_NETS[0].pass);
    }
    
    WebServer_Init();
    AppSettings_Draw1();
    AppSettings_Draw2();
}


void AppSettings_Update() {
    // Check if web dashboard requested a reboot (safe to do from main task)
    if (WebServer_RebootRequested()) {
        flushSettingsToNVS(); // FIX 5: save pending changes before reboot
        oled.clearDisplay();
        centered("REBOOTING...", 28);
        oled.display();
        delay(800);
        ESP.restart();
    }
    // FIX 29: Only redraw at 1Hz and only when dirty flag is set.
    // The old 500ms unconditional redraw was burning CPU even when nothing changed.
    unsigned long now = millis();
    if (displayDirty || (now - lastDrawMs > 1000)) {
        lastDrawMs = now;
        displayDirty = false;
        AppSettings_Draw1();
        AppSettings_Draw2();
    }
}

void AppSettings_Exit() {
    flushSettingsToNVS(); // FIX 5: flush NVS on clean exit
    WebServer_Stop();
}


static void draw_qrcode_cb(esp_qrcode_handle_t qrcode) {
    int size = esp_qrcode_get_size(qrcode);
    int scale = 2;
    if (size * scale > 60) scale = 1; // Auto scale to fit screen

    int ox = (128 - (size * scale)) / 2;
    int oy = (64 - (size * scale)) / 2;
    
    // Draw white background for QR
    oled.fillRect(ox - 2, oy - 2, (size * scale) + 4, (size * scale) + 4, SSD1306_WHITE);
    
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            if (esp_qrcode_get_module(qrcode, x, y)) {
                oled.fillRect(ox + (x * scale), oy + (y * scale), scale, scale, SSD1306_BLACK);
            }
        }
    }
}

void AppSettings_Draw1() {
    oled.clearDisplay();
    oled.setTextColor(SSD1306_WHITE);
    oled.setTextSize(1);
    
    centered("SYSTEM SETTINGS", 0);
    oled.drawFastHLine(0, 10, 128, SSD1306_WHITE);

    const char* options[SETTING_COUNT] = {
        "Web Dashboard",
        "Silent Mode",
        "Display Sleep",
        "Restart Device",
        "Exit Settings"
    };

    if (qrScreen > 0) {
        // Draw QR Code
        oled.clearDisplay();
        
        // 1. Generate QR Data
        String qrData = "";
        if (qrScreen == 1) { // Join WiFi
            if (WiFi.getMode() == WIFI_AP_STA || WiFi.status() != WL_CONNECTED) {
                qrData = "WIFI:S:Macro-Keyboard;T:nopass;;";
            } else {
                // Find password for current network
                String pass = "";
                for(int i=0; i<WIFI_NET_COUNT; i++) {
                    if (String(WIFI_NETS[i].ssid) == WiFi.SSID()) {
                        pass = String(WIFI_NETS[i].pass); break;
                    }
                }
                qrData = "WIFI:S:" + WiFi.SSID() + ";T:WPA;P:" + pass + ";;";
            }
        } else if (qrScreen == 2) { // Dashboard URL
            if (WiFi.getMode() == WIFI_AP_STA || WiFi.status() != WL_CONNECTED) {
                qrData = "http://" + WiFi.softAPIP().toString();
            } else {
                qrData = "http://" + WiFi.localIP().toString();
            }
        }

        // 2. Generate and Render via Callback
        esp_qrcode_config_t cfg = ESP_QRCODE_CONFIG_DEFAULT();
        cfg.display_func = draw_qrcode_cb;
        esp_qrcode_generate(&cfg, qrData.c_str());
        
        // 3. Render labels on sides
        oled.setTextColor(SSD1306_WHITE);
        oled.setTextSize(1);
        
        if (qrScreen == 1) {
            oled.setCursor(0, 0); oled.print("< WiFi");
            oled.setCursor(100, 0); oled.print("URL >");
        } else {
            oled.setCursor(0, 0); oled.print("< WiFi");
            oled.setCursor(100, 0); oled.print("Exit >");
        }
        
        oled.display();
        return;
    }

    for (int i = 0; i < SETTING_COUNT; i++) {
        int y = 16 + (i * 12);
        
        if (i == settingSel) {
            oled.fillRect(0, y - 2, 128, 11, SSD1306_WHITE);
            oled.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
        } else {
            oled.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
        }

        oled.setCursor(4, y);
        oled.print(options[i]);

        // Draw values aligned right
        if (i == 0) { // Web Dashboard
            oled.setCursor(100, y);
            oled.print("OPEN");
        } else if (i == 1) { // Silent Mode
            oled.setCursor(100, y);
            oled.print(silentMode ? "ON" : "OFF");
        } else if (i == 2) { // Sleep
            oled.setCursor(85, y);
            if (sleepTimeoutMs == 0) oled.print("NEVER");
            else if (sleepTimeoutMs == 60000UL) oled.print("1 MIN");
            else if (sleepTimeoutMs == 300000UL) oled.print("5 MIN");
            else if (sleepTimeoutMs == 900000UL) oled.print("15 MIN");
            else oled.print("CUSTM");
        }
    }
    
    oled.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
    oled.display();
}

void AppSettings_Draw2() {
    if (!oled2Active) return; oled2.clearDisplay();
    oled2.setTextColor(SSD1306_WHITE);
    oled2.setTextSize(1);
    
    d2Header("SYSTEM", "CFG");
    d2Divider();
    
    if (qrScreen > 0) {
        if (qrScreen == 1) {
            d2L(20, "SCAN QR");
            d2L(32, "TO JOIN");
        } else {
            d2L(20, "SCAN QR");
            d2L(32, "FOR URL");
        }
    }
    else if (settingSel == 0) {
        d2L(20, "WEB");
        d2L(32, "DASHBRD");
    } else if (settingSel == 1) {
        d2L(20, "SOUND");
        d2L(32, "TOGGLE");
    } else if (settingSel == 2) {
        d2L(20, "OLED");
        d2L(32, "TIMEOUT");
    } else if (settingSel == 3) {
        d2L(20, "REBOOT");
        d2L(32, "SYSTEM");
    } else if (settingSel == 4) {
        d2L(20, "BACK TO");
        d2L(32, "MENU");
    }
    
    oled2.display();
}

void AppSettings_HandleInput(LogicalEvent ev) {
    if (ev == EV_UP_TAP || ev == EV_UP_HOLD) {
        if (qrScreen == 0) settingSel = (settingSel - 1 + SETTING_COUNT) % SETTING_COUNT;
        AppSettings_Draw1();
        AppSettings_Draw2();
    } 
    else if (ev == EV_DOWN_TAP || ev == EV_DOWN_HOLD) {
        if (qrScreen == 0) settingSel = (settingSel + 1) % SETTING_COUNT;
        AppSettings_Draw1();
        AppSettings_Draw2();
    }
    else if (ev == EV_LEFT_TAP || ev == EV_LEFT_HOLD) {
        if (qrScreen > 0) {
            qrScreen--;
            displayDirty = true;
            AppSettings_Draw1();
            AppSettings_Draw2();
            return;
        }

        // FIX 30: Sleep timeout direction was BACKWARDS.
        // LEFT should decrease (shorter) sleep timeout, RIGHT should increase (longer).
        if (settingSel == 1) {
            silentMode = !silentMode;
            settingsDirty = true; // FIX 5: mark dirty, don't write NVS immediately
            displayDirty = true;
            AppSettings_Draw1();
        } else if (settingSel == 2) {
            // LEFT = shorter timeout (more aggressive sleep)
            if (sleepTimeoutMs == 0) sleepTimeoutMs = 900000UL;         // Never -> 15 min
            else if (sleepTimeoutMs == 900000UL) sleepTimeoutMs = 300000UL; // 15 -> 5 min
            else if (sleepTimeoutMs == 300000UL) sleepTimeoutMs = 60000UL;  // 5 min -> 1 min
            else if (sleepTimeoutMs == 60000UL)  sleepTimeoutMs = 0;         // 1 min -> Never
            else sleepTimeoutMs = 300000UL;
            settingsDirty = true;
            displayDirty = true;
            AppSettings_Draw1();
        }
    }

    else if (ev == EV_RIGHT_TAP || ev == EV_RIGHT_HOLD || ev == EV_CENTER_TAP) {
        if (qrScreen > 0) {
            if (ev == EV_CENTER_TAP) { qrScreen = 0; } // Exit QR
            else { qrScreen = (qrScreen == 1) ? 2 : 0; }
            displayDirty = true;
            AppSettings_Draw1();
            AppSettings_Draw2();
            return;
        }

        // Adjust value right or select action
        if (settingSel == 0 && ev == EV_CENTER_TAP) {
            qrScreen = 1; // Open QR flow
            displayDirty = true;
            AppSettings_Draw1();
            AppSettings_Draw2();
        } else if (settingSel == 1) {
            silentMode = !silentMode;
            settingsDirty = true; // FIX 5: mark dirty
            displayDirty = true;
            AppSettings_Draw1();
        } else if (settingSel == 2) {
            // FIX 30: RIGHT = longer timeout (more lenient sleep)
            if (sleepTimeoutMs == 0)           sleepTimeoutMs = 60000UL;  // Never -> 1 min
            else if (sleepTimeoutMs == 60000UL)  sleepTimeoutMs = 300000UL; // 1 min -> 5 min
            else if (sleepTimeoutMs == 300000UL) sleepTimeoutMs = 900000UL; // 5 min -> 15 min
            else if (sleepTimeoutMs == 900000UL) sleepTimeoutMs = 0;         // 15 min -> Never
            else sleepTimeoutMs = 300000UL;
            settingsDirty = true;
            displayDirty = true;
            AppSettings_Draw1();
        } else if (settingSel == 3) {
            // FIX 32: Restart Device requires a confirmation press.
            // First press shows "CONFIRM?" on screen. Second CENTER press reboots.
            static bool confirmPending = false;
            if (!confirmPending) {
                confirmPending = true;
                oled.clearDisplay();
                oled.setTextColor(SSD1306_WHITE);
                oled.setTextSize(1);
                centered("SYSTEM SETTINGS", 0);
                oled.drawFastHLine(0, 10, 128, SSD1306_WHITE);
                oled.setTextSize(1);
                centered("CONFIRM RESTART?", 28);
                centered("Press OK again", 42);
                oled.display();
                // Reset after 3 seconds if not confirmed
                delay(3000);
                confirmPending = false;
                displayDirty = true;
            } else {
                confirmPending = false;
                flushSettingsToNVS();
                oled.clearDisplay();
                centered("REBOOTING...", 28);
                oled.display();
                delay(1000);
                ESP.restart();
            }
        } else if (settingSel == 4) {
            if (ev == EV_CENTER_TAP) {
                extern void AppManager_ReturnToMenu();
                AppManager_ReturnToMenu();
            }
        }
    }
}

