#include "app_settings.h"
#include <Arduino.h>

static int settingSel = 0;
#define SETTING_COUNT 4

void AppSettings_Init() {
    settingSel = 0;
    AppSettings_Draw1();
    AppSettings_Draw2();
}

void AppSettings_Draw1() {
    oled.clearDisplay();
    oled.setTextColor(SSD1306_WHITE);
    oled.setTextSize(1);
    
    centered("SYSTEM SETTINGS", 0);
    oled.drawFastHLine(0, 10, 128, SSD1306_WHITE);

    const char* options[SETTING_COUNT] = {
        "Silent Mode",
        "Display Sleep",
        "Restart Device",
        "Exit Settings"
    };

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
        if (i == 0) { // Silent Mode
            oled.setCursor(100, y);
            oled.print(silentMode ? "ON" : "OFF");
        } else if (i == 1) { // Sleep
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
    oled2.clearDisplay();
    oled2.setTextColor(SSD1306_WHITE);
    oled2.setTextSize(1);
    
    d2Header("SYSTEM", "CFG");
    d2Divider();
    
    if (settingSel == 0) {
        d2L(20, "SOUND");
        d2L(32, "TOGGLE");
    } else if (settingSel == 1) {
        d2L(20, "OLED");
        d2L(32, "TIMEOUT");
    } else if (settingSel == 2) {
        d2L(20, "REBOOT");
        d2L(32, "SYSTEM");
    } else if (settingSel == 3) {
        d2L(20, "BACK TO");
        d2L(32, "MENU");
    }
    
    oled2.display();
}

void AppSettings_HandleInput(LogicalEvent ev) {
    if (ev == EV_UP_TAP || ev == EV_UP_HOLD) {
        settingSel = (settingSel - 1 + SETTING_COUNT) % SETTING_COUNT;
        AppSettings_Draw1();
        AppSettings_Draw2();
    } 
    else if (ev == EV_DOWN_TAP || ev == EV_DOWN_HOLD) {
        settingSel = (settingSel + 1) % SETTING_COUNT;
        AppSettings_Draw1();
        AppSettings_Draw2();
    }
    else if (ev == EV_LEFT_TAP || ev == EV_LEFT_HOLD) {
        // Adjust value left
        if (settingSel == 0) {
            silentMode = !silentMode;
            AppSettings_Draw1();
        } else if (settingSel == 1) {
            if (sleepTimeoutMs == 900000UL) sleepTimeoutMs = 300000UL;
            else if (sleepTimeoutMs == 300000UL) sleepTimeoutMs = 60000UL;
            else if (sleepTimeoutMs == 60000UL) sleepTimeoutMs = 0; // Never
            else if (sleepTimeoutMs == 0) sleepTimeoutMs = 900000UL;
            else sleepTimeoutMs = 300000UL; // Default recovery
            AppSettings_Draw1();
        }
    }
    else if (ev == EV_RIGHT_TAP || ev == EV_RIGHT_HOLD || ev == EV_CENTER_TAP) {
        // Adjust value right or select action
        if (settingSel == 0) {
            silentMode = !silentMode;
            AppSettings_Draw1();
        } else if (settingSel == 1) {
            if (sleepTimeoutMs == 0) sleepTimeoutMs = 60000UL;
            else if (sleepTimeoutMs == 60000UL) sleepTimeoutMs = 300000UL;
            else if (sleepTimeoutMs == 300000UL) sleepTimeoutMs = 900000UL;
            else if (sleepTimeoutMs == 900000UL) sleepTimeoutMs = 0; // Never
            else sleepTimeoutMs = 300000UL; // Default recovery
            AppSettings_Draw1();
        } else if (settingSel == 2) {
            if (ev == EV_CENTER_TAP) {
                oled.clearDisplay();
                centered("REBOOTING...", 28);
                oled.display();
                delay(1000);
                ESP.restart();
            }
        } else if (settingSel == 3) {
            if (ev == EV_CENTER_TAP) {
                extern void AppManager_ReturnToMenu();
                AppManager_ReturnToMenu();
            }
        }
    }
}
