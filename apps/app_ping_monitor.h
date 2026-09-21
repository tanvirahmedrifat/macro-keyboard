#ifndef APP_PING_MONITOR_H
#define APP_PING_MONITOR_H

// ─────────────────────────────────────────────────────────────────────────────
//  PING MONITOR — Public API
//  Layer 9  |  Non-blocking millis() state machine
//  Reuses: oled/oled2, LogicalEvent, WIFI_NETS[], beep helpers
//
//  REQUIRES: "ESP32Ping" library by dvarrel
//    Arduino IDE → Library Manager → search "ESP32Ping" → Install
// ─────────────────────────────────────────────────────────────────────────────

#include "app_manager.h"

void AppPingMonitor_Init();
void AppPingMonitor_Update();
void AppPingMonitor_HandleEvent(LogicalEvent ev);
void AppPingMonitor_Exit();

#endif // APP_PING_MONITOR_H
