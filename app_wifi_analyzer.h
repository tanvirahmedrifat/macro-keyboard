#ifndef APP_WIFI_ANALYZER_H
#define APP_WIFI_ANALYZER_H

#include "globals.h"
#include <esp_wifi.h>

void AppWifiAnalyzer_Init();
void AppWifiAnalyzer_Update();
void AppWifiAnalyzer_Exit();
bool AppWifiAnalyzer_IsActive();

#endif
