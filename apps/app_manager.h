#ifndef APP_MANAGER_H
#define APP_MANAGER_H

#include "system_input.h"

enum RadioReq {
    RADIO_NONE,
    RADIO_BLE,
    RADIO_WIFI
};

struct AppContainer {
    RadioReq radio;
    void (*init)();
    void (*exit)();
};

void AppManager_Init();
void AppManager_Update();
void AppManager_HandleEvent(LogicalEvent ev);
void AppManager_HandleMatrix(char mKey);
void AppManager_ReturnToMenu();
void AppManager_SwitchApp(int layerIndex);

#endif
