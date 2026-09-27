#include "app_manager.h"
#include "app_wifi.h"
#include "app_ios_macro.h"
#include "app_nokia.h"
#include "app_media.h"
#include "app_games.h"
#include "app_settings.h"
#include "app_tester.h"
#include "app_ping_monitor.h"

extern void BootMenu_HandleInput(LogicalEvent ev);

static RadioReq currentRadio = RADIO_BLE; // System boots with BLE enabled

static AppContainer apps[10] = {
    // FIX BUG-A: Layer 0 (menu) needs RADIO_BLE, not RADIO_NONE.
    // With RADIO_NONE, returning to the boot menu called ble.kill() and left BLE dead
    // until the user re-entered a BLE app. All menu-layer apps are BLE apps.
    {RADIO_BLE,  nullptr,             nullptr},             // 0 (Boot Menu)
    {RADIO_BLE,  nullptr,             nullptr},             // 1 (iOS Macro)
    {RADIO_WIFI, AppWifi_Init,        AppWifi_Exit},        // 2 (WiFi Analyzer)
    {RADIO_BLE,  nullptr,             nullptr},             // 3 (Nokia)
    {RADIO_BLE,  nullptr,             nullptr},             // 4 (Media)
    {RADIO_NONE, nullptr,             nullptr},             // 5
    {RADIO_NONE, AppGames_Init,       AppGames_ExitToMenu}, // 6 (Games)
    {RADIO_WIFI,  AppSettings_Init,    AppSettings_Exit},    // 7 (Settings)
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
            // FIX BUG-D: If a macro is mid-execution when ble.kill() runs,
            // the macro's MDELAY calls ble.releaseAll() on a destroyed stack → crash.
            // Signal the macro to abort FIRST so it can call ble.releaseAll() cleanly,
            // then wait a brief moment for it to wind down before killing the stack.
            extern volatile bool macroAborted;
            macroAborted = true;
            delay(30); // allow macroDelay() one 10ms tick to see the flag and return
            ble.releaseAll(); // ensure no keys are stuck before destroying stack
            ble.kill();       // Completely de-initialize NimBLE to free radio
            delay(80);        // Give RF controller time to flush
        } else if (currentRadio == RADIO_WIFI) {
            WiFi.disconnect(true);
            WiFi.mode(WIFI_OFF);
            delay(80);  // Give RF controller time to flush before BLE starts
        }
        
        // Start next radio
        if (nextRadio == RADIO_BLE) {
            // FIX BUG-B: ble.kill() destroys NimBLE stack state including security config.
            // ble.begin() alone re-inits NimBLE but with DEFAULT settings.
            // Must reapply security + address config so iOS accepts the existing HID bond.
            // Without this, iOS sees a device with different capabilities and rejects pairing.
            ble.setSecurityMode(HIDSecurity::JustWorks);
            ble.setRandomAddress(false);
            ble.setLogLevel(HIDLogLevel::Normal);
            ble.begin(); // Cleanly re-inits with correct config
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
    if (activeLayer == 1) {
        AppIosMacro_Update();
    } else if (activeLayer == 2) {
        AppWifi_Update();
    } else if (activeLayer == 3) {
        AppNokia_Update();
    } else if (activeLayer == 6) {
        AppGames_Update();
    } else if (activeLayer == 7) {
        AppSettings_Update();
    } else if (activeLayer == 9) {
        AppPingMonitor_Update();
    }
}

void AppManager_HandleMatrix(char mKey) {
    // Always reset idle timer on any physical button press
    resetIdle();

    // ── Boot Menu: ignore matrix keys while in menu ──
    if (inBootMenu) {
        return;
    }

    bool handled = false;
    if (activeLayer == 1) {
        AppIosMacro_HandleMatrix(mKey);
        handled = true;
    } else if (activeLayer == 3) {
        AppNokia_HandleMatrix(mKey);
        handled = true;
    } else if (activeLayer == 8) {
        AppTester_HandleMatrix(mKey);
        handled = true;
    }

    // FIX 34: Only play beepTap() if a real app handled the key.
    // Previously, even unassigned layers (like layer 5) would play a beep,
    // giving misleading positive feedback for a no-op press.
    if (handled) {
        // FIX 3: Don't double-beep for layer 1 (iOS Macro).
        // AppIosMacro_HandleMatrix() calls beepDone() internally at the end
        // of every macro. Calling beepTap() here on top causes a double beep.
        if (activeLayer != 1) {
            beepTap();
        }
    }
}

void AppManager_ReturnToMenu() {
    beepTap();
    
    // Switching to Layer 0 (RADIO_NONE) cleanly shuts down the current app 
    // and turns off all radios, placing the device in an idle state.
    AppManager_SwitchApp(0);
    
    // Override back to the Boot Menu
    inBootMenu = true;
    
    // FIX: CRITICAL — Reset the idle timer BEFORE waiting for key release.
    // If the user was inside an app for more than 15 seconds, idleStartTime is stale.
    // Without this reset, updateBootMenu() would immediately enter the screensaver
    // instead of drawing the menu, making it look like the screen is broken.
    // Also wakes OLEDs if they were sleeping inside the app.
    resetIdle();
    wakeDisplays();
    
    drawAction(">> menu");
    
    // Wait for all main buttons and matrix keypad keys to be physically released
    // to prevent the newly-opened menu from instantly consuming the remaining hold.
    WaitAllKeysReleased();
    delay(50); // Small debounce after physical release
    
    // Immediately draw the menu so OLED 1 is correct from the first frame back.
    // Without this, there is a 1-frame window where the menu loop hasn't run yet
    // and OLED 1 would show a black screen or the action toast aftermath.
    extern void drawMenuList();
    drawMenuList();
}


void AppManager_HandleEvent(LogicalEvent ev) {
    // Wake display on any action
    resetIdle();

    // ── Boot Menu: highest priority — swallow all events before apps see them ──
    if (inBootMenu) {
        BootMenu_HandleInput(ev);
        return;
    }

    // ── Layer 1, 2, and 9: forward events and prevent default global shortcuts
    // For iOS Macro (1), we swallow directional/center button events so they don't beep or trigger display cycle.
    if (activeLayer == 1 || activeLayer == 2 || activeLayer == 9) {
        if (activeLayer != 1) { // Normal beep behavior for Wifi/Ping
            if (ev == EV_UP_HOLD    || ev == EV_DOWN_HOLD  || ev == EV_LEFT_HOLD ||
                ev == EV_RIGHT_HOLD || ev == EV_CENTER_HOLD) {
                beepHoldReady();
            } else if (ev != EV_NONE && ev != EV_BACKSPACE_HOLD_2S) {
                beepTap();
            }
        }
        
        if (activeLayer == 1) AppIosMacro_HandleEvent(ev);
        else if (activeLayer == 2) AppWifi_HandleEvent(ev);
        else if (activeLayer == 9) {
            AppPingMonitor_HandleEvent(ev);
            // BUG-FIX: PingMonitor handles EV_BACKSPACE_HOLD_2S internally (calls ReturnToMenu itself).
            // Calling ReturnToMenu again here caused a double-invocation: double beep + double key wait.
            return; // skip outer ReturnToMenu for layer 9
        }
        
        // Still allow returning to menu globally (layers 1 and 2)
        if (ev == EV_BACKSPACE_HOLD_2S) {
            AppManager_ReturnToMenu();
        }
        return;
    }

    switch (ev) {
        case EV_UP_TAP:
            beepTap();
            if (activeLayer == 3) AppNokia_Btn1_Tap();
            else if (activeLayer == 4) AppMedia_Btn1_Tap();
            else if (activeLayer == 6) AppGames_HandleInput(ev);
            else if (activeLayer == 7) AppSettings_HandleInput(ev);
            else if (activeLayer == 8) AppTester_Btn1_Tap();
            break;
            
        case EV_UP_HOLD:
            beepHoldReady();
            if (activeLayer == 3) AppNokia_Btn1_Hold();
            else if (activeLayer == 4) AppMedia_Btn1_Hold();
            else if (activeLayer == 6) AppGames_HandleInput(ev);
            else if (activeLayer == 7) AppSettings_HandleInput(ev);
            break;
            
        case EV_DOWN_TAP:
            beepTap();
            if (activeLayer == 3) AppNokia_Btn2_Tap();
            else if (activeLayer == 4) AppMedia_Btn2_Tap();
            else if (activeLayer == 6) AppGames_HandleInput(ev);
            else if (activeLayer == 7) AppSettings_HandleInput(ev);
            else if (activeLayer == 8) AppTester_Btn2_Tap();
            break;
            
        case EV_DOWN_HOLD:
            beepHoldReady();
            if (activeLayer == 3) AppNokia_Btn2_Hold();
            else if (activeLayer == 4) AppMedia_Btn2_Hold();
            else if (activeLayer == 6) AppGames_HandleInput(ev);
            else if (activeLayer == 7) AppSettings_HandleInput(ev);
            break;
            
        case EV_LEFT_TAP:
            beepTap();
            if (activeLayer == 3) AppNokia_Btn3_Tap();
            else if (activeLayer == 4) AppMedia_Btn3_Tap();
            else if (activeLayer == 6) AppGames_HandleInput(ev);
            else if (activeLayer == 7) AppSettings_HandleInput(ev);
            else if (activeLayer == 8) AppTester_Btn3_Tap();
            break;
            
        case EV_LEFT_HOLD:
            beepHoldReady();
            if (activeLayer == 3) AppNokia_Btn3_Hold();
            else if (activeLayer == 4) AppMedia_Btn3_Hold();
            else if (activeLayer == 6) AppGames_HandleInput(ev);
            else if (activeLayer == 7) AppSettings_HandleInput(ev);
            break;
            
        case EV_RIGHT_TAP:
            beepTap();
            if (activeLayer == 3) AppNokia_Btn4_Tap();
            else if (activeLayer == 4) AppMedia_Btn4_Tap();
            else if (activeLayer == 6) AppGames_HandleInput(ev);
            else if (activeLayer == 7) AppSettings_HandleInput(ev);
            else if (activeLayer == 8) AppTester_Btn4_Tap();
            break;
            
        case EV_RIGHT_HOLD:
            beepHoldReady();
            if (activeLayer == 3) AppNokia_Btn4_Hold();
            else if (activeLayer == 4) AppMedia_Btn4_Hold();
            else if (activeLayer == 6) AppGames_HandleInput(ev);
            else if (activeLayer == 7) AppSettings_HandleInput(ev);
            break;
            
        case EV_CENTER_TAP:
            beepTap();
            if (activeLayer == 3) AppNokia_Btn5_Tap();
            // BUG-38 FIX: Media layer (4) had no center-tap handler — silent dead button.
            // Added Btn5_Tap for Media (typically play/pause or next track).
            else if (activeLayer == 4) AppMedia_Btn5_Tap();
            else if (activeLayer == 6) AppGames_HandleInput(ev);
            else if (activeLayer == 7) AppSettings_HandleInput(ev);
            else if (activeLayer == 8) AppTester_Btn5_Tap();
            break;
            
        case EV_CENTER_HOLD:
            beepHoldReady();
            dispState = (dispState + 1) % 4; // 0=Both, 1=D1, 2=D2, 3=Off
            setDisplays();
            if (dispState == 0) { drawAction(">> Both ON"); }
            else if (dispState == 1) { drawAction(">> D1 ON"); }
            else if (dispState == 2) { drawAction(">> D2 ON"); }
            else if (dispState == 3) { drawAction(">> Displays OFF"); }
            resetIdle();
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
            // FIX: Redraw the correct app UI after Bad Apple ends.
            // Without this, the last video frame is permanently stuck on both OLEDs
            // until the next user input triggers a redraw via tickIdle().
            resetIdle();
            d1Draw();
            d2Idle();
            break;
            
        case EV_NONE:
        default:
            break;
    }
}

