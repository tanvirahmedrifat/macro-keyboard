#include "app_nokia.h"

int t9Mode = 0; // 0=abc, 1=ABC, 2=123
char t9LastKey = 0;
int t9TapCount = 0;
unsigned long t9LastTime = 0;
char t9Buffer[21] = {0};
const unsigned long T9_TIMEOUT = 800;

#define MDELAY(x) if(macroDelay(x)) { ble.releaseAll(); resetIdle(); return; }

// ── T9 KEYBOARD FUNCTIONS ───────────────────────────────────────────────────
void d1DrawT9() {
  if (dispState == 2 || dispState == 3) return;
  
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  
  oled.setTextSize(1);
  centered("L3: T9 KEYBOARD", 4);
  oled.drawLine(10, 15, SCREEN_W - 10, 15, SSD1306_WHITE);
  
  oled.setTextSize(2);
  oled.setCursor(0, 20);
  
  oled.print(t9Buffer);
  if ((millis() / 400) % 2 == 0) oled.print("_");
  
  oled.display();
}

void d2DrawT9() {
  if (dispState == 1 || dispState == 3) return;
  
  oled2.clearDisplay();
  oled2.setTextColor(SSD1306_WHITE);
  oled2.setTextSize(1);
  
  d2Header("T9", "MODE");
  d2Divider();
  
  if (t9LastKey != 0) {
    oled2.setTextSize(2);
    char lk[2] = {t9LastKey, 0};
    d2L(26, lk);
  }
  
  oled2.setTextSize(1);
  const char* modeStr = t9Mode == 0 ? "abc" : (t9Mode == 1 ? "ABC" : "123");
  int w = strlen(modeStr) * 6;
  oled2.setCursor(73 + max(0, (35 - w) / 2), 26);
  oled2.print(modeStr);
  
  oled2.display();
}

const char* getT9Sequence(char key) {
  if (t9Mode == 2) {
    switch(key) {
      case '1': return "1"; case '2': return "2"; case '3': return "3";
      case '4': return "4"; case '5': return "5"; case '6': return "6";
      case '7': return "7"; case '8': return "8"; case '9': return "9";
      case '0': return "0";
      default: return "";
    }
  }
  bool up = (t9Mode == 1);
  switch (key) {
    case '1': return ".,?!-'@1";
    case '2': return up ? "ABC2" : "abc2";
    case '3': return up ? "DEF3" : "def3";
    case '4': return up ? "GHI4" : "ghi4";
    case '5': return up ? "JKL5" : "jkl5";
    case '6': return up ? "MNO6" : "mno6";
    case '7': return up ? "PQRS7" : "pqrs7";
    case '8': return up ? "TUV8" : "tuv8";
    case '9': return up ? "WXYZ9" : "wxyz9";
    case '0': return " 0";
    default: return "";
  }
}

void updateT9Timeout() {
  if (t9LastKey != 0 && (millis() - t9LastTime) >= T9_TIMEOUT) {
    t9LastKey = 0;
  }
}

void handleT9KeyPress(char mKey) {
  if (mKey == '#') {
    t9Mode = (t9Mode + 1) % 3;
    t9LastKey = 0; 
    beepTap();
    resetIdle();
    return;
  }
  
  if (mKey == '*') {
    t9LastKey = 0;
    int len = strlen(t9Buffer);
    if (len > 0) t9Buffer[len - 1] = '\0';
    ble.tap(KEY_BACKSPACE);
    beepTap();
    resetIdle();
    return;
  }

  const char* seq = getT9Sequence(mKey);
  if (strlen(seq) == 0) return;

  unsigned long now = millis();
  
  if (mKey == t9LastKey && (now - t9LastTime) < T9_TIMEOUT) {
    t9TapCount++;
    if (t9TapCount >= strlen(seq)) t9TapCount = 0;
    int len = strlen(t9Buffer);
    if (len > 0) t9Buffer[len - 1] = '\0';
    ble.tap(KEY_BACKSPACE);
  } else {
    t9LastKey = mKey;
    t9TapCount = 0;
  }
  
  t9LastTime = now;
  char c = seq[t9TapCount];
  int len = strlen(t9Buffer);
  if (len < 20) {
    t9Buffer[len] = c;
    t9Buffer[len + 1] = '\0';
  } else {
    memmove(t9Buffer, t9Buffer + 1, 19);
    t9Buffer[19] = c;
    t9Buffer[20] = '\0';
  }
  
  if (c == ' ') ble.tap(KEY_SPACE);
  else {
    char str[2] = {c, '\0'};
    ble.print(str);
  }
  
  beepTap();
  resetIdle();
}



void AppNokia_Update() {
  updateT9Timeout();
}

void AppNokia_HandleMatrix(char mKey) {
  handleT9KeyPress(mKey);
}

void AppNokia_Draw1() {
  d1DrawT9();
}

void AppNokia_Draw2() {
  d2DrawT9();
}

void AppNokia_Btn1_Hold() {
  drawAction(">> minimize");
  ble.tap(KEY_M, KEY_MOD_LGUI);
  MDELAY(400);
}

void AppNokia_Btn1_Tap() {
  drawAction(">> up arrow");
  t9Buffer[0] = '\0';
  ble.tap(KEY_UP);
  MDELAY(200);
}

void AppNokia_Btn2_Hold() {
  drawAction(">> maximize");
  ble.tap(KEY_UP, KEY_MOD_LGUI);
  MDELAY(400);
}

void AppNokia_Btn2_Tap() {
  drawAction(">> down arrow");
  t9Buffer[0] = '\0';
  ble.tap(KEY_DOWN);
  MDELAY(200);
}

void AppNokia_Btn3_Hold() {
  drawAction(">> git push");
  ble.print("git add . && git commit -m 'update' && git push");
  MDELAY(500);
  ble.tap(KEY_RETURN);
  MDELAY(400);
}

void AppNokia_Btn3_Tap() {
  drawAction(">> left arrow");
  t9Buffer[0] = '\0';
  ble.tap(KEY_LEFT);
  MDELAY(200);
}

void AppNokia_Btn4_Hold() {
  drawAction(">> switch app");
  ble.tap(KEY_TAB, KEY_MOD_LGUI);
  MDELAY(400);
}

void AppNokia_Btn4_Tap() {
  drawAction(">> right arrow");
  t9Buffer[0] = '\0';
  ble.tap(KEY_RIGHT);
  MDELAY(200);
}

void AppNokia_Btn5_Tap() {
  if (t9LastKey != 0) {
    t9LastKey = 0; // Commit current key
  }
  t9Buffer[0] = '\0'; // Clear buffer on Enter to prevent text scrolling off-screen
  drawAction(">> enter");
  ble.tap(KEY_RETURN);
  MDELAY(200);
}
