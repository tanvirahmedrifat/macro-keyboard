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
  
  // FIX 15 & 18: Clamp the visible buffer to the last 10 characters
  // (10 chars * 12px TextSize2 = 120px, safely fits on 128px screen).
  // Show a '...' prefix if text has been scrolled off the left edge.
  // Also print a char counter (e.g. 14/20) in the top-right corner.
  int bufLen = strlen(t9Buffer);
  oled.setTextSize(1);
  char counter[8];
  snprintf(counter, sizeof(counter), "%d/20", bufLen);
  int cw = strlen(counter) * 6;
  oled.setCursor(SCREEN_W - cw - 2, 5);
  oled.print(counter);
  
  // Determine the visible window (last 10 chars)
  const char* visPtr = t9Buffer;
  bool truncated = false;
  if (bufLen > 10) {
    visPtr = t9Buffer + (bufLen - 10);
    truncated = true;
  }
  
  oled.setTextSize(2);
  oled.setCursor(truncated ? 14 : 0, 20);
  if (truncated) {
    oled.print("<");
  }
  oled.print(visPtr);
  if ((millis() / 400) % 2 == 0) oled.print("_");
  
  oled.display();
}


void d2DrawT9() {
  if (dispState == 1 || dispState == 3) return;
  
  if (!oled2Active) return; oled2.clearDisplay();
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
  
  // FIX 19: Show spacebar hint so user knows how to type a space
  oled2.setTextSize(1);
  d2R(44, "0=Spc");
  
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
    beepTap(); // FIX 17: backspace must give audio feedback
    resetIdle();
    return;
  }

  const char* seq = getT9Sequence(mKey);
  if (strlen(seq) == 0) return;

  unsigned long now = millis();
  
  // FIX 16: Cross-key finalization.
  // If the user presses a DIFFERENT key before the timeout, the previous
  // character should be finalized immediately rather than waiting 800ms.
  // The old code only checked (mKey == t9LastKey), so pressing '2' then '3'
  // immediately would reset the cycle on '3' but the '2' char was already
  // committed to the BLE stream — the buffer and BLE were actually already
  // correct. The real fix here is that t9LastKey is set to the new key,
  // resetting t9TapCount to 0, which naturally starts fresh on the new key.
  // We also explicitly finalize (clear t9LastKey) so the timeout doesn't fire
  // stale data after we've moved on.
  if (mKey != t9LastKey) {
    // Different key pressed: finalize the previous char (no BLE needed,
    // the char is already in the stream) and start fresh on the new key.
    t9LastKey = mKey;
    t9TapCount = 0;
  } else if ((now - t9LastTime) < T9_TIMEOUT) {
    // Same key within timeout window: cycle to next character.
    t9TapCount++;
    if (t9TapCount >= (int)strlen(seq)) t9TapCount = 0;
    int len = strlen(t9Buffer);
    if (len > 0) t9Buffer[len - 1] = '\0';
    ble.tap(KEY_BACKSPACE);
  } else {
    // Same key but timeout expired: treat as new press.
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
