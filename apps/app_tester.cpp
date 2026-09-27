#include "app_tester.h"

#define MDELAY(x) if(macroDelay(x)) { ble.releaseAll(); resetIdle(); return; }

// FIX: Draw a live button map on OLED 2 so users can see which buttons are
// being tested. Right zone shows the last pressed button for immediate feedback.
static char lastBtnName[12] = "---";

static void drawTesterLayout(const char* btnName) {
  strncpy(lastBtnName, btnName, 11);
  lastBtnName[11] = '\0';
  if (!oled2Active) return;
  oled2.clearDisplay();
  oled2.setTextColor(SSD1306_WHITE);
  oled2.setTextSize(1);
  d2Header("TESTER", "MAP");
  d2Divider();
  d2L(18, "U-D-L-R");
  d2L(28, "CTR=OK");
  d2L(38, "Mat:1-9");
  d2R(18, "LAST:");
  d2R(28, lastBtnName);
  oled2.display();
}

void AppTester_Btn1_Tap() {
  drawAction(">> btn 1 up");
  drawTesterLayout("BTN1-UP");
  ble.print("button 1 up\n");
  MDELAY(200);
}

void AppTester_Btn2_Tap() {
  drawAction(">> btn 2 down");
  drawTesterLayout("BTN2-DN");
  ble.print("button 2 down\n");
  MDELAY(200);
}

void AppTester_Btn3_Tap() {
  drawAction(">> btn 3 left");
  drawTesterLayout("BTN3-LT");
  ble.print("button 3 left\n");
  MDELAY(200);
}

void AppTester_Btn4_Tap() {
  drawAction(">> btn 4 right");
  drawTesterLayout("BTN4-RT");
  ble.print("button 4 right\n");
  MDELAY(200);
}

void AppTester_Btn5_Tap() {
  drawAction(">> btn 5 center");
  drawTesterLayout("BTN5-CT");
  ble.print("button 5 center\n");
  MDELAY(200);
}

void AppTester_HandleMatrix(char mKey) {
  char msg[32];
  snprintf(msg, sizeof(msg), ">> matrix %c", mKey);
  drawAction(msg);
  char layoutLabel[12];
  snprintf(layoutLabel, sizeof(layoutLabel), "MAT-%c", mKey);
  drawTesterLayout(layoutLabel);
  char printMsg[32];
  snprintf(printMsg, sizeof(printMsg), "matrix %c pressed\n", mKey);
  ble.print(printMsg);
  MDELAY(200);
}

