#ifndef APP_WIFI_H
#define APP_WIFI_H

#include "globals.h"

#include "system_input.h"

void AppWifi_Init();
void AppWifi_Update();
void AppWifi_HandleEvent(LogicalEvent ev);
void AppWifi_Exit();

#endif
