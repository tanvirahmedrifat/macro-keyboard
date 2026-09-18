


#include "app_manager.h"
#include "app_wifi.h"
#include "app_ios_macro.h"
#include "app_nokia.h"
#include "app_media.h"
#include "app_tester.h"
#include "app_games.h"
#include "app_ping_monitor.h"

extern void BootMenu_HandleInput(LogicalEvent ev);

static RadioReq currentRadio = RADIO_BLE; // System boots with BLE enabled

static AppContainer apps[10] = {
    {RADIO_NONE, nullptr,             nullptr},             // 0
    {RADIO_BLE,  nullptr,             nullptr},             // 1 (iOS Macro)
    {RADIO_WIFI, AppWifi_Init,        AppWifi_Exit},        // 2 (WiFi Analyzer)
    {RADIO_BLE,  nullptr,             nullptr},             // 3 (Nokia)
    {RADIO_BLE,  nullptr,             nullptr},             // 4 (Media)
    {RADIO_NONE, nullptr,             nullptr},             // 5
    {RADIO_NONE, AppGames_Init,       AppGames_ExitToMenu}, // 6 (Games)
    {RADIO_BLE,  nullptr,             nullptr},             // 7 (Setting)
    {RADIO_BLE,  nullptr,             nullptr},             // 8 (Tester)
    {RADIO_WIFI, AppPingMonitor_Init, AppPingMonitor_Exit}  // 9 (Ping Monitor)
};

void AppManager_Init() {
    // activeLayer is handled by menu layer switching
}

void AppManager_SwitchApp(int layerIndex) {
    // 1. Exit current app
    if (activeLayer >= 0 && activeLayer <= 9 && apps[activeLayer].exit) {
        apps[activeLayer].exit();
    }
    
    RadioReq nextRadio = apps[layerIndex].radio;
    
    // 2. Handle Radio Transition
    if (currentRadio != nextRadio) {
        // Shutdown current radio safely
        if (currentRadio == RADIO_BLE) {
            ble.end();
            delay(50);
        } else if (currentRadio == RADIO_WIFI) {
            WiFi.disconnect(true);
            WiFi.mode(WIFI_OFF);
            delay(50);
        }
        
        // Start next radio
        if (nextRadio == RADIO_BLE) {
            ble.begin();
        } else if (nextRadio == RADIO_WIFI) {
            WiFi.mode(WIFI_STA);
        }
        currentRadio = nextRadio;
    }
    
    // 3. Switch state
    activeLayer = layerIndex;
    inBootMenu = false; // We are now in an app
    
    // 4. Init next app
    if (apps[activeLayer].init) {
        apps[activeLayer].init();
    }
}

void AppManager_Update() {
    if (activeLayer == 2) {
        AppWifi_Update();
    } else if (activeLayer == 3) {
        AppNokia_Update();
    } else if (activeLayer == 6) {
        AppGames_Update();
    } else if (activeLayer == 9) {
        AppPingMonitor_Update();
    }
}

void AppManager_HandleMatrix(char mKey) {
    resetIdle();
    beepTap();
    if (activeLayer == 3) {
        AppNokia_HandleMatrix(mKey);
    } else if (activeLayer == 8) {
        AppTester_HandleMatrix(mKey);
    }
}

void AppManager_ReturnToMenu() {
    beepTap();
    
    // Switching to Layer 1 (default BLE layer) cleanly shuts down the current app 
    // and handles radio transitions via the container framework.
    AppManager_SwitchApp(1);
    
    // Override back to the Boot Menu
    inBootMenu = true;
    
    drawAction(">> menu");
    ble.print("returned to menu\n");
    
    // Wait for all main buttons and matrix keypad keys to be physically released
    // to prevent the newly-opened menu from instantly consuming the remaining hold.
    WaitAllKeysReleased();
    delay(50); // Small debounce after physical release
}

void AppManager_HandleEvent(LogicalEvent ev) {
    // Wake display on any action
    resetIdle();

    // ── Layer 2 (WiFi Analyzer) and Layer 9 (Ping Monitor): forward all events, including BACKSPACE_HOLD_2S
    // Prevents global shortcuts (display cycle, Bad Apple) from firing inside the app.
    if (activeLayer == 2 || activeLayer == 9) {
        if (ev == EV_UP_HOLD    || ev == EV_DOWN_HOLD  || ev == EV_LEFT_HOLD ||
            ev == EV_RIGHT_HOLD || ev == EV_CENTER_HOLD) {
            beepHoldReady();
        } else if (ev != EV_NONE) {
            beepTap();
        }
        
        if (activeLayer == 2) AppWifi_HandleEvent(ev);
        else if (activeLayer == 9) AppPingMonitor_HandleEvent(ev);
        
        return;
    }

    switch (ev) {
        case EV_UP_TAP:
            if (activeLayer == 0) BootMenu_HandleInput(ev);
            else {
                beepTap();
                if (activeLayer == 1) AppIosMacro_Btn1_Tap();
                else if (activeLayer == 3) AppNokia_Btn1_Tap();
                else if (activeLayer == 4) AppMedia_Btn1_Tap();
                else if (activeLayer == 6) AppGames_HandleInput(ev);
                else if (activeLayer == 8) AppTester_Btn1_Tap();
            }
            break;
            
        case EV_UP_HOLD:
            if (activeLayer == 0) BootMenu_HandleInput(ev);
            else {
                beepHoldReady();
                if (activeLayer == 1) AppIosMacro_Btn1_Hold();
                else if (activeLayer == 3) AppNokia_Btn1_Hold();
                else if (activeLayer == 4) AppMedia_Btn1_Hold();
                else if (activeLayer == 6) AppGames_HandleInput(ev);
            }
            break;
            
        case EV_DOWN_TAP:
            if (activeLayer == 0) BootMenu_HandleInput(ev);
            else {
                beepTap();
                if (activeLayer == 1) AppIosMacro_Btn2_Tap();
                else if (activeLayer == 3) AppNokia_Btn2_Tap();
                else if (activeLayer == 4) AppMedia_Btn2_Tap();
                else if (activeLayer == 6) AppGames_HandleInput(ev);
                else if (activeLayer == 8) AppTester_Btn2_Tap();
            }
            break;
            
        case EV_DOWN_HOLD:
            if (activeLayer == 0) BootMenu_HandleInput(ev);
            else {
                beepHoldReady();
                if (activeLayer == 1) AppIosMacro_Btn2_Hold();
                else if (activeLayer == 3) AppNokia_Btn2_Hold();
                else if (activeLayer == 4) AppMedia_Btn2_Hold();
                else if (activeLayer == 6) AppGames_HandleInput(ev);
            }
            break;
            
        case EV_LEFT_TAP:
            if (activeLayer == 0) BootMenu_HandleInput(ev);
            else {
                beepTap();
                if (activeLayer == 1) AppIosMacro_Btn3_Tap();
                else if (activeLayer == 3) AppNokia_Btn3_Tap();
                else if (activeLayer == 4) AppMedia_Btn3_Tap();
                else if (activeLayer == 6) AppGames_HandleInput(ev);
                else if (activeLayer == 8) AppTester_Btn3_Tap();
            }
            break;
            
        case EV_LEFT_HOLD:
            if (activeLayer == 0) BootMenu_HandleInput(ev);
            else {
                beepHoldReady();
                if (activeLayer == 1) AppIosMacro_Btn3_Hold();
                else if (activeLayer == 3) AppNokia_Btn3_Hold();
                else if (activeLayer == 4) AppMedia_Btn3_Hold();
                else if (activeLayer == 6) AppGames_HandleInput(ev);
            }
            break;
            
        case EV_RIGHT_TAP:
            if (activeLayer == 0) BootMenu_HandleInput(ev);
            else {
                beepTap();
                if (activeLayer == 1) AppIosMacro_Btn4_Tap();
                else if (activeLayer == 3) AppNokia_Btn4_Tap();
                else if (activeLayer == 4) AppMedia_Btn4_Tap();
                else if (activeLayer == 6) AppGames_HandleInput(ev);
                else if (activeLayer == 8) AppTester_Btn4_Tap();
            }
            break;
            
        case EV_RIGHT_HOLD:
            if (activeLayer == 0) BootMenu_HandleInput(ev);
            else {
                beepHoldReady();
                if (activeLayer == 1) AppIosMacro_Btn4_Hold();
                else if (activeLayer == 3) AppNokia_Btn4_Hold();
                else if (activeLayer == 4) AppMedia_Btn4_Hold();
                else if (activeLayer == 6) AppGames_HandleInput(ev);
            }
            break;
            
        case EV_CENTER_TAP:
            if (activeLayer == 0) BootMenu_HandleInput(ev);
            else {
                beepTap();
                if (activeLayer == 3) AppNokia_Btn5_Tap();
                else if (activeLayer == 6) AppGames_HandleInput(ev);
                else if (activeLayer == 8) AppTester_Btn5_Tap();
            }
            break;
            
        case EV_CENTER_HOLD:
            if (activeLayer == 0) BootMenu_HandleInput(ev);
            else {
                beepHoldReady();
                dispState = (dispState + 1) % 4; // 0=Both, 1=D1, 2=D2, 3=Off
                setDisplays();
                if (dispState == 0) { drawAction(">> Both ON"); }
                else if (dispState == 1) { drawAction(">> D1 ON"); }
                else if (dispState == 2) { drawAction(">> D2 ON"); }
                else if (dispState == 3) { drawAction(">> Displays OFF"); }
                resetIdle();
            }
            break;
            
        case EV_BACKSPACE_HOLD_2S:
            AppManager_ReturnToMenu();
            break;

        case EV_CENTER_HOLD_5S:
            dispState = 0;          // Force both displays ON
            setDisplays();
            wakeDisplays();
            drawAction(">> bad apple!");
            delay(300);             // plain delay — no macro context, no abort risk
            playBadApple(&oled, &oled2);
            resetIdle();
            break;
            
        case EV_NONE:
        default:
            break;
    }
}
