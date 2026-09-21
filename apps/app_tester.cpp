#include "app_tester.h"

#define MDELAY(x) if(macroDelay(x)) { ble.releaseAll(); resetIdle(); return; }

void AppTester_Btn1_Tap() {
  drawAction(">> btn 1 up");
  ble.print("button 1 up\n");
  MDELAY(200);
}

void AppTester_Btn2_Tap() {
  drawAction(">> btn 2 down");
  ble.print("button 2 down\n");
  MDELAY(200);
}

void AppTester_Btn3_Tap() {
  drawAction(">> btn 3 left");
  ble.print("button 3 left\n");
  MDELAY(200);
}

void AppTester_Btn4_Tap() {
  drawAction(">> btn 4 right");
  ble.print("button 4 right\n");
  MDELAY(200);
}

void AppTester_Btn5_Tap() {
  drawAction(">> btn 5 center");
  ble.print("button 5 center\n");
  MDELAY(200);
}

void AppTester_HandleMatrix(char mKey) {
  char msg[32];
  snprintf(msg, sizeof(msg), ">> matrix %c", mKey);
  drawAction(msg);
  char printMsg[32];
  snprintf(printMsg, sizeof(printMsg), "matrix %c pressed\n", mKey);
  ble.print(printMsg);
  MDELAY(200);
}
