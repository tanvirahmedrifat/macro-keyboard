#ifndef APP_IOS_MACRO_H
#define APP_IOS_MACRO_H

#include "globals.h"
#include "../system/system_input.h"

void AppIosMacro_Key3();
void AppIosMacro_Key1();
void AppIosMacro_Key0();
void AppIosMacro_Key2();
void AppIosMacro_Key5();
void AppIosMacro_Key4();
void AppIosMacro_Key7();
void AppIosMacro_Key6();

void AppIosMacro_HandleMatrix(char mKey);
void AppIosMacro_Update();
void AppIosMacro_HandleEvent(LogicalEvent ev);

#endif
