// ============================================================
//  ESP32 Professional Macro Keyboard  v3.0
//  Owner: Rifat
//  ─────────────────────────────────────────────────
//  BTN1  tap    → first name (lowercase) + Enter
//  BTN1  hold   → firstname+lastname+random digits (lowercase)
//  BTN2  tap    → form-filling macro (DOB + gender)
//  BTN2  hold   → NEW profile (new name + new password)
//  BTN3  tap    → type current password (human-speed) + Enter
//  BTN3  hold   → Open Notes via Spotlight → Paste → Enter → Password → Enter → Home Screen
//  BTN4  tap    → Spotlight -> type "clear history" -> Enter
//  BTN4  hold   → Spotlight -> type "Date & Time" -> Enter
// ============================================================

#include <HijelHID_BLEKeyboard.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "app_manager.h"
#include <Preferences.h>
#include <qrcode.h>
#include "names_data.h"
#include "BadApple.h"
#include "menu_icons.h"
#include "app_wifi.h"
#include "app_ios_macro.h"
#include "app_nokia.h"
#include "app_media.h"
#include "app_tester.h"
#include "app_games.h"
#include "system_input.h"
#include "app_manager.h"
#include "app_ping_monitor.h"
#include <Keypad.h>

// ── MATRIX KEYPAD (3x4) ──────────────────────────────────────────────────
const byte MATRIX_ROWS = 4;
const byte MATRIX_COLS = 3;
char matrixKeys[MATRIX_ROWS][MATRIX_COLS] = {
  {'1','2','3'},
  {'4','5','6'},
  {'7','8','9'},
  {'*','0','#'}
};
byte matrixRowPins[MATRIX_ROWS] = {19, 18, 5, 16};
byte matrixColPins[MATRIX_COLS] = {4, 25, 23};
Keypad matrixPad = Keypad(makeKeymap(matrixKeys), matrixRowPins, matrixColPins, MATRIX_ROWS, MATRIX_COLS);


// ── IDLE / WEATHER STATE ──────────────────────────────────
unsigned long idleStartTime = 0;
float weatherTemp = 0.0;
float weatherWind = 0.0;
int weatherCode = -1; // -1 means stale/unfetched

// ── OLED ──────────────────────────────────────────────────
#define SCREEN_W   128
#define SCREEN_H    64
#define OLED_RST    -1
#define OLED_ADDR  0x3C

// Display 1: Black/White Main UI (SSD1306) on Second I2C (D32, D33)
#define SDA_2 32
#define SCL_2 33
TwoWire I2C_2 = TwoWire(1);
Adafruit_SSD1306 oled(SCREEN_W, SCREEN_H, &I2C_2, OLED_RST);

// Display 2: Blue/Yellow Big Text (SSD1306) on Default I2C (D21, D22)
Adafruit_SSD1306 oled2(SCREEN_W, SCREEN_H, &Wire, OLED_RST);

// ── BLE ───────────────────────────────────────────────────
HijelHID_BLEKeyboard ble("Tanvir's Keyboard", "Rifat-Dev");

// ── PINS ──────────────────────────────────────────────────
const int PIN1 = 12, PIN2 = 13, PIN3 = 14, PIN4 = 26, PIN5 = 27;
const int BUZZ  = 17;              // passive buzzer (D17)
const unsigned long BTN_HOLD_MS = 2000;

// ── PASSWORD CHARSET (universal — works on every platform) ──
// Only uses: A-Z  a-z  0-9  ! @ # $ - _
const char UPPER[]   = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
const char LOWER_C[] = "abcdefghijklmnopqrstuvwxyz";
const char NUMS[]    = "0123456789";
const char SPEC[]    = "!@#$-_";
const char ALL[]     = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!@#$-_";
const int  ALL_LEN   = 68;
const int  PWD_LEN   = 16;

// ── STATE ─────────────────────────────────────────────────
int  fnIdx = 0, lnIdx = 0;   // independent first/last name indices
char pwd[PWD_LEN + 1];
bool wasCon = false;

// ── T9 KEYBOARD STATE ──────────────────────────────────────



// ── WIFI & NTP (dynamic list from Preferences) ────────────────────────
WifiCred WIFI_NETS[10]; // Max 10 networks
int WIFI_NET_COUNT = 0;
Preferences prefs;
const char* WIFI_SSID = WIFI_NETS[0].ssid; // used for display only (updated at runtime)

const char* NTP_SERVER   = "pool.ntp.org";
const long  GMT_OFFSET_S = 6L * 3600L;  // UTC+6 Bangladesh
const int   DST_OFFSET_S = 0;
bool        ntpSynced    = false;
unsigned long wifiStartMs = 0;  // tracks when WiFi was started (for timeout)
bool        d2HoldPwd    = false; // tracks if password should be kept on D2
int         dispState    = 0;     // 0=both ON, 1=D1 ON/D2 OFF, 2=D1 OFF/D2 ON, 3=both OFF
int         activeLayer  = 1;     // 1=Profiles, 2=Media, 3=Coding

// ── BOOT MENU STATE ──────────────────────────────────────────────────────
bool inBootMenu = true;
int menuSel = 0;       
int menuScroll = 0;
const int MENU_COUNT = 8;
const char* menuNames[8] = {"WIFI ANALYZER", "IOS Macro", "Nokia Keyboard", "Media Mode", "Games", "Settings", "Button Tester", "PING MONITOR"};
const int menuLayers[8] = {2, 1, 3, 4, 6, 7, 8, 9};

// ── OLED AUTO-SLEEP (Burn-in Protection) ─────────────────────────────────
// After SLEEP_AFTER_MS of inactivity, both OLEDs are powered off completely.
// Any button press instantly wakes them. This protects the OLED panels from
// permanent phosphor burn-in caused by long-running static images.
bool          oledSleeping  = false;
bool          silentMode    = false;
unsigned long sleepTimeoutMs = 5UL * 60UL * 1000UL;  // 5 minutes (mutable)

// ── IDLE DISPLAY STATE ──────────────────────────────────────────────────
unsigned long drawTimer = 0;
const unsigned long DRAW_RATE = 120;  // ~8 FPS for blink effects

void getTimeCStr(char* buf) {
  if (!ntpSynced) {
    strcpy(buf, "00:00--");
    return;
  }
  struct tm t;
  if (!getLocalTime(&t)) {
    strcpy(buf, "00:00--");
    return;
  }
  strftime(buf, 9, "%I:%M%p", &t); // 12-hr e.g. "04:40PM"
}

// ── STRING HELPERS ────────────────────────────────────────
String firstName() {
  String s = String(firstNames[fnIdx]);
  s.toLowerCase();
  return s;
}
String lastName() {
  String s = String(lastNames[lnIdx]);
  s.toLowerCase();
  return s;
}

// ── PASSWORD GENERATOR ────────────────────────────────────
void genPwd() {
  // XOR multiple noise sources for stronger entropy
  randomSeed(analogRead(0) ^ analogRead(1) ^ analogRead(2) ^ millis() ^ (millis() * 31337UL));
  // Guarantee one from each class at positions 0-3
  pwd[0] = UPPER[random(0, 26)];
  pwd[1] = LOWER_C[random(0, 26)];
  pwd[2] = NUMS[random(0, 10)];
  pwd[3] = SPEC[random(0, 6)];
  // Fill rest from full charset
  for (int i = 4; i < PWD_LEN; i++)
    pwd[i] = ALL[random(0, ALL_LEN)];
  // Fisher-Yates shuffle
  for (int i = PWD_LEN - 1; i > 0; i--) {
    int j = random(0, i + 1);
    char t = pwd[i]; pwd[i] = pwd[j]; pwd[j] = t;
  }
  pwd[PWD_LEN] = '\0';
}

void newProfile() {
  int pf = fnIdx, pl = lnIdx;
  // Randomize both independently until at least one changes
  while (fnIdx == pf && lnIdx == pl) {
    fnIdx = random(0, NAME_COUNT);
    lnIdx = random(0, NAME_COUNT);
  }
  genPwd();
  d2HoldPwd = false; // clear static password on screen when profile changes
}

void setDisplays() {
  switch (dispState) {
    case 0:
      oled.ssd1306_command(SSD1306_DISPLAYON);
      oled2.ssd1306_command(SSD1306_DISPLAYON);
      break;
    case 1:
      oled.ssd1306_command(SSD1306_DISPLAYON);
      oled2.ssd1306_command(SSD1306_DISPLAYOFF);
      break;
    case 2:
      oled.ssd1306_command(SSD1306_DISPLAYOFF);
      oled2.ssd1306_command(SSD1306_DISPLAYON);
      break;
    case 3:
      oled.ssd1306_command(SSD1306_DISPLAYOFF);
      oled2.ssd1306_command(SSD1306_DISPLAYOFF);
      break;
  }
}

// Wake both OLEDs from auto-sleep, honouring the user's dispState setting.
void wakeDisplays() {
  if (!oledSleeping) return;
  oledSleeping = false;
  setDisplays();  // re-applies the correct ON/OFF per dispState
}

// Sleep both OLEDs regardless of dispState (burn-in protection).
void sleepDisplays() {
  if (oledSleeping) return;
  oledSleeping = true;
  oled.ssd1306_command(SSD1306_DISPLAYOFF);
  oled2.ssd1306_command(SSD1306_DISPLAYOFF);
}

// ── TYPING HELPERS ────────────────────────────────────────

// Gaussian-like distribution using Central Limit Theorem (4-sample average).
// Produces a bell-curve shape instead of a flat uniform distribution —
// values near 'mean' are far more likely than extremes.
int gaussRandom(int mean, int spread) {
  long s = random(-spread, spread) + random(-spread, spread)
         + random(-spread, spread) + random(-spread, spread);
  return constrain(mean + (int)(s / 4), mean - spread, mean + spread);
}

// Returns true for the top-20 English digraphs.
// Humans type these pairs faster due to muscle memory (e.g. 'th', 'er', 'in').
bool isFastDigraph(char a, char b) {
  char la = (a >= 'A' && a <= 'Z') ? (char)(a + 32) : a;
  char lb = (b >= 'A' && b <= 'Z') ? (char)(b + 32) : b;
  const char fast[][2] = {
    {'t','h'},{'h','e'},{'i','n'},{'e','r'},{'a','n'},
    {'r','e'},{'o','n'},{'e','n'},{'a','t'},{'e','s'},
    {'s','t'},{'t','e'},{'i','s'},{'o','u'},{'a','r'},
    {'a','s'},{'i','t'},{'h','a'},{'e','t'},{'i','o'}
  };
  for (int i = 0; i < 20; i++)
    if (la == fast[i][0] && lb == fast[i][1]) return true;
  return false;
}

// Characters that require Shift — humans slow down slightly before pressing them.
bool needsShift(char c) {
  return (c >= 'A' && c <= 'Z') || c == '!' || c == '@' || c == '#' || c == '$';
}

// Returns a random physically adjacent key on a standard QWERTY keyboard
char getAdjacentKey(char c) {
  c = tolower(c);
  switch (c) {
    case 'q': { const char adj[] = "wa"; return adj[random(2)]; }
    case 'w': { const char adj[] = "qeas"; return adj[random(4)]; }
    case 'e': { const char adj[] = "wrsd"; return adj[random(4)]; }
    case 'r': { const char adj[] = "etdf"; return adj[random(4)]; }
    case 't': { const char adj[] = "ryfg"; return adj[random(4)]; }
    case 'y': { const char adj[] = "tugh"; return adj[random(4)]; }
    case 'u': { const char adj[] = "yihj"; return adj[random(4)]; }
    case 'i': { const char adj[] = "uojk"; return adj[random(4)]; }
    case 'o': { const char adj[] = "ipkl"; return adj[random(4)]; }
    case 'p': { const char adj[] = "ol"; return adj[random(2)]; }
    case 'a': { const char adj[] = "qwsz"; return adj[random(4)]; }
    case 's': { const char adj[] = "awedzx"; return adj[random(6)]; }
    case 'd': { const char adj[] = "serfcx"; return adj[random(6)]; }
    case 'f': { const char adj[] = "drtgvc"; return adj[random(6)]; }
    case 'g': { const char adj[] = "ftyhbv"; return adj[random(6)]; }
    case 'h': { const char adj[] = "gyujnb"; return adj[random(6)]; }
    case 'j': { const char adj[] = "huikmn"; return adj[random(6)]; }
    case 'k': { const char adj[] = "jiolm"; return adj[random(5)]; }
    case 'l': { const char adj[] = "kop"; return adj[random(3)]; }
    case 'z': { const char adj[] = "asx"; return adj[random(3)]; }
    case 'x': { const char adj[] = "zsdc"; return adj[random(4)]; }
    case 'c': { const char adj[] = "xdfv"; return adj[random(4)]; }
    case 'v': { const char adj[] = "cfgb"; return adj[random(4)]; }
    case 'b': { const char adj[] = "vghn"; return adj[random(4)]; }
    case 'n': { const char adj[] = "bhjm"; return adj[random(4)]; }
    case 'm': { const char adj[] = "njk"; return adj[random(3)]; }
    default: return (c == 'z') ? 'x' : (char)(c + 1); // Fallback
  }
}

// ── MACRO ABORT / PANIC STOP ────────────────────────────────
volatile bool macroAborted = false;
unsigned long macroStartMs = 0;

// Custom delay that constantly checks if a button was pressed
// Returns true if the macro should be aborted immediately
bool macroDelay(int ms) {
  unsigned long start = millis();
  while (millis() - start < (unsigned long)ms) {
    if (macroAborted) return true;
    if (!ble.isPaired()) { macroAborted = true; return true; }
    // Any button press after the 300ms grace period aborts the macro
    if (millis() - macroStartMs > 300) {
      if (digitalRead(PIN1) == LOW || digitalRead(PIN2) == LOW ||
          digitalRead(PIN3) == LOW || digitalRead(PIN4) == LOW ||
          digitalRead(PIN5) == LOW) {
        macroAborted = true;
        return true;
      }
    }
    delay(10);
  }
  return false;
}

// ── HUMAN TYPING ──────────────────────────────────────────
void humanType(const char* text) {
  if (macroDelay(gaussRandom(130, 50))) return; // pre-typing cognitive delay

  int  len  = strlen(text);
  char prev = 0;

  for (int i = 0; i < len; i++) {
    // ── Per-character abort check (runs before every keystroke) ───────────
    if (macroAborted) { ble.releaseAll(); return; }
    if (!ble.isPaired()) { macroAborted = true; return; }

    char c = text[i];

    // ① Base timings — Gaussian bell-curve (not flat uniform)
    // Calibrated for 38-45 WPM (approx 266-316 ms per character)
    int hold = gaussRandom(90, 20);
    int gap  = gaussRandom(200, 40);

    // ② Common digraph → speed up (muscle memory effect)
    if (prev && isalpha(prev) && isalpha(c) && isFastDigraph(prev, c)) {
      hold -= random(10, 30);
      gap  -= random(20, 60);
    }

    // ③ Shift-key character → extra cognitive load, slow down
    if (needsShift(c)) {
      gap  += random(40, 100);
      hold += random(10, 30);
    }

    // ④ Space (word boundary) → natural micro-pause
    if (c == ' ')    gap += random(50, 120);
    // ⑤ First char after a space → hesitation starting new word
    if (prev == ' ') gap += random(30, 80);

    // ⑥ Hesitation pause (~8% chance — simulates a "thinking" moment)
    if (random(100) < 8) gap += gaussRandom(350, 120);

    // ⑦ Muscle-memory fast burst (~4% chance — practiced sub-sequences)
    if (random(100) < 4) {
      hold = gaussRandom(60, 15);
      gap  = gaussRandom(80, 20);
    }

    // ⑧ Typo + backspace (~5% on alpha chars, skip first char)
    if (i > 0 && isalpha(c) && random(100) < 5) {
      char typo = getAdjacentKey(c);
      if (isupper(c) && random(100) < 80) typo = toupper(typo);
      ble.setTapDelay(gaussRandom(90, 20));
      ble.setKeyGap(1);
      ble.print(typo);
      if (macroDelay(gaussRandom(400, 120))) { ble.releaseAll(); return; }
      ble.tap(KEY_BACKSPACE, 0, gaussRandom(80, 20), 0);
      if (macroDelay(gaussRandom(300, 90)))  { ble.releaseAll(); return; }
    }

    // ⑨ Typing fatigue: subtle slowdown accumulates every ~8 chars
    int fatigue = (i / 8) * (int)random(2, 6);
    hold = constrain(hold + fatigue / 2, 25, 300);
    gap  = constrain(gap  + fatigue,     10, 800);

    // ── KEY PRESS ─────────────────────────────────────────────────────────
    // ble.print(c) handles ASCII→HID conversion (uppercase, shift, symbols).
    // keyGap=1ms so library returns after hold+1ms; our macroDelay(gap) then
    // handles the interruptible inter-key pause — abort within ~10ms.
    ble.setTapDelay(hold);
    ble.setKeyGap(1);   // 1ms internal gap so library returns fast
    ble.print(c);       // proper ASCII→HID conversion (handles uppercase/shift)
    if (macroDelay(gap)) { ble.releaseAll(); return; } // interruptible gap

    prev = c;
  }

  // Reset to library defaults
  ble.setTapDelay(25);
  ble.setKeyGap(25);
  macroDelay(gaussRandom(160, 55)); // post-typing settling delay (also interruptible)
}

void humanTap(uint8_t key) {
  ble.tap(key, 0, gaussRandom(90, 20), gaussRandom(150, 40));
}

// ── BUZZER SOUNDS ─────────────────────────────────────────
// Uses direct GPIO bit-banging instead of tone()/LEDC so the
// BLE stack cannot interfere with the hardware timer.
void buzzNote(int freq, int ms) {
  if (silentMode) return;
  if (freq <= 0 || ms <= 0) return;
  int halfUs = 500000 / freq;            // half-period in microseconds
  long cycles = (long)freq * ms / 1000; // total on/off cycles needed
  for (long i = 0; i < cycles; i++) {
    digitalWrite(BUZZ, HIGH); delayMicroseconds(halfUs);
    digitalWrite(BUZZ, LOW);  delayMicroseconds(halfUs);
  }
}

TaskHandle_t beepTaskHandle = NULL;

void beepTask(void* param) {
  int type = (int)(intptr_t)param;
  switch (type) {
    case 1: // Tap
      buzzNote(1800, 55);
      break;
    case 2: // Hold Ready
      buzzNote(2200, 70); vTaskDelay(pdMS_TO_TICKS(110)); 
      buzzNote(2200, 70);
      break;
    case 3: // Done
      buzzNote(1400, 60); vTaskDelay(pdMS_TO_TICKS(90)); 
      buzzNote(2600, 100);
      break;
    case 4: // New Profile
      buzzNote(1047, 80); vTaskDelay(pdMS_TO_TICKS(110));
      buzzNote(1319, 80); vTaskDelay(pdMS_TO_TICKS(110));
      buzzNote(1568, 120);
      break;
    case 5: // Connect
      buzzNote(1047, 70); vTaskDelay(pdMS_TO_TICKS(100));
      buzzNote(1319, 70); vTaskDelay(pdMS_TO_TICKS(100));
      buzzNote(1568, 70); vTaskDelay(pdMS_TO_TICKS(100));
      buzzNote(2093, 120);
      break;
    case 6: // Disconnect
      buzzNote(1200, 90); vTaskDelay(pdMS_TO_TICKS(120));
      buzzNote(900, 90); vTaskDelay(pdMS_TO_TICKS(120));
      buzzNote(600, 150);
      break;
    case 7: // Error
      buzzNote(800, 120); vTaskDelay(pdMS_TO_TICKS(160)); 
      buzzNote(800, 120);
      break;
    case 8: // Boot
      buzzNote(1760, 60);
      break;
  }
  
  beepTaskHandle = NULL;
  vTaskDelete(NULL);
}

void playBeep(int type) {
  if (silentMode) return;
  if (beepTaskHandle != NULL) {
    vTaskDelete(beepTaskHandle);
    digitalWrite(BUZZ, LOW);
    beepTaskHandle = NULL;
  }
  // ── Fix 1: Pin buzzer task to Core 0 ─────────────────────────────────────
  // The BLE/NimBLE stack runs exclusively on Core 1. By pinning the bit-bang
  // buzzer loop to Core 0 we give BLE 100% of Core 1 during any jingle,
  // preventing the supervision timeout that causes iOS to drop the HID link.
  // Stack bumped from 1024 → 1536 words for the longer multi-note jingles.
  // Priority 1 → 0 so the idle task can still run on Core 0 between notes.
  xTaskCreatePinnedToCore(beepTask, "beep", 1536, (void*)(intptr_t)type, 0, &beepTaskHandle, 0);
}

// ── WEATHER FETCH (RUNS ONCE AT BOOT) ─────────────────────
void fetchWeatherOnce() {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  // Use http:// to avoid massive TLS memory overhead on the ESP32
  http.begin("http://api.open-meteo.com/v1/forecast?latitude=22.8098&longitude=89.5644&current_weather=true");
  http.setTimeout(5000); // 5-second hard timeout — never hang boot
  int httpCode = http.GET();
  if (httpCode == 200) {
    String payload = http.getString();
    // Static doc sized for the Open-Meteo response (~512 bytes of JSON fields)
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (!error) {
      weatherTemp = doc["current_weather"]["temperature"].as<float>();
      weatherWind = doc["current_weather"]["windspeed"].as<float>();
      weatherCode = doc["current_weather"]["weathercode"].as<int>();
    }
  }
  http.end();
}

void beepTap() { playBeep(1); }
void beepHoldReady() { playBeep(2); }
void beepDone() { playBeep(3); }
void beepNewProfile() { playBeep(4); }
void beepConnect() { playBeep(5); }
void beepDisconnect() { playBeep(6); }
void beepError() { playBeep(7); }
void beepBoot() { playBeep(8); }

// ── DISPLAY HELPERS ─────────────────────────────────────────

// D1: centered text helper (still used in bootAnim)
void centered(const char* txt, int y, int size) {
  oled.setTextSize(size);
  int w = strlen(txt) * 6 * size;
  oled.setCursor((SCREEN_W - w) / 2, y);
  oled.print(txt);
}

// ── DISPLAY 2 SAFE-ZONE HELPERS ─────────────────────────────
// Dead X cols: 5,6,16,18,20,72,108,112,114,116,121,126,127
// Dead Y rows: 55,57,59
// Left safe : X:22–71  (8 chars at size-1 = 48px)
// Right safe: X:73–107 (5 chars at size-1 = 30px)
// Safe Y    : 0–54

void d2L(int y, const char* txt) {
  char buf[9]; strncpy(buf, txt, 8); buf[8] = '\0';
  oled2.setCursor(22, y); oled2.print(buf);
}
void d2R(int y, const char* txt) {
  char buf[6]; strncpy(buf, txt, 5); buf[5] = '\0';
  oled2.setCursor(73, y); oled2.print(buf);
}
void d2Divider() {
  oled2.drawFastVLine(71, 16, 38, SSD1306_WHITE); // sits at X=71, dodges dead col 72
}
void d2Header(const char* left, const char* right) {
  d2L(4, left); d2R(4, right);
  oled2.drawLine(22, 14, 107, 14, SSD1306_WHITE); // divides yellow from blue section
}

// ── DISPLAY 1: STATUS SCREEN (B/W, full 128x64) ──
void d1Draw(const char* msg = "", bool showPwd = false) {
  if (dispState == 2 || dispState == 3) return; // Skip rendering if display is off

  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);

  // D1 is the Profile & Action Display.
  oled.setTextSize(1);
  char pf[32];
  if (activeLayer == 1)      snprintf(pf, sizeof(pf), "L1: PROFILES (%d)", fnIdx + 1);
  else if (activeLayer == 3) snprintf(pf, sizeof(pf), "L3: PRESENT");
  else if (activeLayer == 4) snprintf(pf, sizeof(pf), "L4: MEDIA");
  else if (activeLayer == 6) snprintf(pf, sizeof(pf), "L6: GAMES");
  else if (activeLayer == 8) snprintf(pf, sizeof(pf), "L8: TESTER");
  else                       snprintf(pf, sizeof(pf), "L?: UNKNOWN");
  oled.setCursor((SCREEN_W - strlen(pf) * 6) / 2, 4); 
  oled.print(pf);
  oled.drawLine(10, 15, SCREEN_W - 10, 15, SSD1306_WHITE);

  // Profile Name (Stacked to avoid cropping)
  String fn = firstName();
  String ln = lastName();
  if (activeLayer == 3) { fn = "WINDOW"; ln = "MACROS"; }
  if (activeLayer == 4) { fn = "MEDIA"; ln = "MACROS"; }
  if (activeLayer == 7) { fn = "SYSTEM"; ln = "SETTINGS"; }
  if (activeLayer == 8) {
    fn = "TOOLS"; ln = "TESTER";
  }
  
  oled.setTextSize(2);
  oled.setCursor((SCREEN_W - fn.length() * 12) / 2, 16);
  oled.print(fn);
  
  oled.setCursor((SCREEN_W - ln.length() * 12) / 2, 34);
  oled.print(ln);
  
  // Action / Message Area
  if (showPwd) {
    oled.setTextSize(1);
    oled.drawLine(10, 52, SCREEN_W - 10, 52, SSD1306_WHITE);
    centered(pwd, 55);
  } else if (strlen(msg) > 0) {
    oled.setTextSize(1);
    oled.drawLine(10, 52, SCREEN_W - 10, 52, SSD1306_WHITE);
    centered(msg, 55);
  } else if (!ble.isConnected()) {
    oled.setTextSize(1);
    oled.drawLine(10, 52, SCREEN_W - 10, 52, SSD1306_WHITE);
    if ((millis() / 500) % 2 == 0) centered("WAITING FOR BLE...", 55);
  }
  // When connected and idle, we display NOTHING at the bottom, 
  // giving the Profile Name maximum breathing room.

  oled.display();
}

// ── DISPLAY 2: TYPING VIEW ──
void d2Typing(const char* hdr_l, const char* hdr_r,
              const char* line1, const char* line2 = "", bool liveBlink = true) {
  if (dispState == 1 || dispState == 3) return; // Skip rendering if display is off

  oled2.clearDisplay();
  oled2.setTextColor(SSD1306_WHITE);
  
  oled2.setTextSize(1);
  d2Header(hdr_l, hdr_r);
  d2Divider();
  
  // Password lines at size 1 (8 chars = 48px, fits left safe zone)
  d2L(20, line1);
  if (strlen(line2) > 0) d2L(32, line2);
  
  // Live indicator on right safe zone
  if (liveBlink) {
    if ((millis() / 200) % 2 == 0) d2R(24, "LIVE");
  } else {
    d2R(24, " OK."); // Static text when holding the password on screen
  }
  oled2.display();
}

// ── DISPLAY 2: IDLE STATE ──
void d2Idle() {
  if (dispState == 1 || dispState == 3) return; // Skip rendering if display is off

  if (d2HoldPwd && ble.isConnected()) {
    char h1[9], h2[9];
    strncpy(h1, pwd, 8); h1[8] = '\0';
    strncpy(h2, pwd + 8, 8); h2[8] = '\0';
    
    char ts[9];
    getTimeCStr(ts);
    
    // Call d2Typing but mark it as NOT live (static display)
    d2Typing(ts, "STAT", h1, h2, false);
    return;
  }

  oled2.clearDisplay();
  oled2.setTextColor(SSD1306_WHITE);

  char ts[9];
  getTimeCStr(ts);
  
  // Yellow Header
  oled2.setTextSize(1);
  d2Header(ts, ble.isConnected() ? " BLE" : "!BLE");
  d2Divider();

  // Blue Area (Left Safe: 50px, Right Safe: 35px)
  if (!ble.isConnected()) {
    oled2.setTextSize(2);
    // "WAIT" at size 2 is 4 chars * 12px = 48px (Fits perfectly in 50px left safe zone)
    oled2.setCursor(23, 24); oled2.print("WAIT");
    
    // Large blinking block on right safe zone
    if ((millis() / 500) % 2 == 0) oled2.fillRect(78, 26, 12, 12, SSD1306_WHITE);
  } else {
    oled2.setTextSize(2);
    // "RDY." at size 2 is 4 chars * 12px = 48px
    oled2.setCursor(23, 24); oled2.print("RDY.");
    
    // Large blinking block on right safe zone
    if ((millis() / 500) % 2 == 0) oled2.fillRect(78, 26, 12, 12, SSD1306_WHITE);
  }
  oled2.display();
}



// \u2500\u2500 SHOW PASSWORD ON BOTH DISPLAYS WHILE TYPING \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500
void drawPasswordTyping() {
  char h1[9], h2[9];
  strncpy(h1, pwd, 8); h1[8] = '\0';
  strncpy(h2, pwd + 8, 8); h2[8] = '\0';
  d1Draw("", true);          // D1: password in content area
  d2Typing("TYPING:", "PWD", h1, h2); // D2: password in safe zones
}

static char actionToastMsg[32] = {0};
static unsigned long actionToastEnd = 0;

void drawAction(const char* msg) {
  strncpy(actionToastMsg, msg, 31);
  actionToastMsg[31] = '\0';
  actionToastEnd = millis() + 600;
  _drawActionInternal();
}

void _drawActionInternal() {
  d1Draw(actionToastMsg);  // D1: action msg in content area
  
  // D2: action in safe zones
  oled2.clearDisplay();
  oled2.setTextColor(SSD1306_WHITE);
  oled2.setTextSize(1);
  d2Header("ACTION", "CMD");
  d2Divider();
  
  oled2.setTextSize(2);
  oled2.setCursor(23, 24); oled2.print("RUN!");
  
  if ((millis() / 200) % 2 == 0) oled2.fillRect(78, 28, 6, 6, SSD1306_WHITE);
  oled2.display();
}

// \u2500\u2500 IDLE REFRESH \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500
void resetIdle() {
  drawTimer = 0;
  idleStartTime = millis();
  wakeDisplays();  // any activity wakes the OLEDs if they were sleeping
}

void d1Screensaver(unsigned long now) {
  if (dispState == 2 || dispState == 3) return;
  oled.clearDisplay();
  
  static unsigned long stateTime = 0;
  static unsigned long stateDelay = 2000;
  static int state = 0; // 0=Center, 1=Blink, 2=Left, 3=Right, 4=Happy, 5=Angry
  static int nextState = 0;
  
  if (now - stateTime > stateDelay) {
    if (state == 1) { 
       state = nextState; // restore previous state after blink
       stateTime = now;
       stateDelay = random(1200, 3500); 
    } else {
       int r = random(100);
       if (r < 35) { nextState = state; state = 1; stateDelay = 150; } // Blink duration (150ms)
       else if (r < 50) { state = 0; stateDelay = random(1200, 3500); } // Center
       else if (r < 65) { state = 2; stateDelay = random(1200, 3500); } // Left
       else if (r < 80) { state = 3; stateDelay = random(1200, 3500); } // Right
       else if (r < 90) { state = 4; stateDelay = random(1200, 3500); } // Happy
       else { state = 5; stateDelay = random(1200, 3500); } // Angry
       
       stateTime = now;
    }
  }
  
  int eyeW = 32;
  int eyeH = 40;
  
  int lx = 24;
  int rx = 72;
  int ly = 12;
  int ry = 12;
  int lh = eyeH;
  int rh = eyeH;
  
  if (state == 1) { // Blink
    ly += (eyeH/2) - 4;
    ry += (eyeH/2) - 4;
    lh = 8;
    rh = 8;
  } else if (state == 2) { // Left
    lx -= 12; rx -= 12;
  } else if (state == 3) { // Right
    lx += 12; rx += 12;
  }
  
  // Draw base eyes
  oled.fillRoundRect(lx, ly, eyeW, lh, 8, SSD1306_WHITE);
  oled.fillRoundRect(rx, ry, eyeW, rh, 8, SSD1306_WHITE);
  
  // Expressions
  if (state == 4) { // Happy (cheeks cover bottom)
    oled.fillRoundRect(lx-4, ly + (lh/2) + 4, eyeW+8, lh/2, 10, SSD1306_BLACK);
    oled.fillRoundRect(rx-4, ry + (rh/2) + 4, eyeW+8, rh/2, 10, SSD1306_BLACK);
  } else if (state == 5) { // Angry (slanted brows)
    oled.fillTriangle(lx-4, ly-4, lx+eyeW+8, ly-4, lx+eyeW+8, ly+16, SSD1306_BLACK);
    oled.fillTriangle(rx+eyeW+4, ly-4, rx-8, ly-4, rx-8, ly+16, SSD1306_BLACK);
  }

  oled.display();
}

void drawWeatherIcon(Adafruit_SSD1306* disp, int x, int y, int wmo, int hour) {
  bool isNight = (hour < 6 || hour >= 18);
  
  auto drawSun = [&](int sx, int sy) {
    disp->fillCircle(sx, sy, 5, SSD1306_WHITE);
    // simple thick rays
    disp->fillRect(sx-1, sy-9, 3, 3, SSD1306_WHITE); // N
    disp->fillRect(sx-1, sy+7, 3, 3, SSD1306_WHITE); // S
    disp->fillRect(sx-9, sy-1, 3, 3, SSD1306_WHITE); // W
    disp->fillRect(sx+7, sy-1, 3, 3, SSD1306_WHITE); // E
    
    // Diagonal thick rays
    for(int i=-1; i<=0; i++) {
      disp->drawLine(sx-5+i, sy-5, sx-7+i, sy-7, SSD1306_WHITE);
      disp->drawLine(sx+5+i, sy-5, sx+7+i, sy-7, SSD1306_WHITE);
      disp->drawLine(sx-5+i, sy+5, sx-7+i, sy+7, SSD1306_WHITE);
      disp->drawLine(sx+5+i, sy+5, sx+7+i, sy+7, SSD1306_WHITE);
    }
  };
  
  auto drawMoon = [&](int sx, int sy) {
    disp->fillCircle(sx, sy, 7, SSD1306_WHITE);
    disp->fillCircle(sx-3, sy-2, 6, SSD1306_BLACK); // chunky crescent cutout facing right
  };
  
  auto drawCloud = [&](int cx, int cy) {
    // Black cutout border for clean overlapping
    disp->fillCircle(cx-5, cy+3, 6, SSD1306_BLACK);
    disp->fillCircle(cx+2, cy-1, 8, SSD1306_BLACK);
    disp->fillCircle(cx+9, cy+3, 6, SSD1306_BLACK);
    disp->fillRect(cx-5, cy, 14, 10, SSD1306_BLACK);
    // Solid white cloud
    disp->fillCircle(cx-5, cy+3, 4, SSD1306_WHITE);
    disp->fillCircle(cx+2, cy-1, 6, SSD1306_WHITE);
    disp->fillCircle(cx+9, cy+3, 4, SSD1306_WHITE);
    disp->fillRect(cx-5, cy+1, 14, 7, SSD1306_WHITE);
  };

  if (wmo >= 50 && wmo <= 67) {
    // Rain
    drawCloud(x+14, y+8);
    for(int i=0; i<3; i++) {
      int dx = x + 9 + i*5;
      disp->drawLine(dx, y+18, dx-2, y+22, SSD1306_WHITE);
      disp->drawLine(dx+1, y+18, dx-1, y+22, SSD1306_WHITE);
    }
  } else if (wmo == 3) {
    // Overcast (Cloud only)
    drawCloud(x+14, y+12);
  } else if (wmo == 1 || wmo == 2) {
    // Partly Cloudy (Sun/Moon popping out top right)
    if (isNight) drawMoon(x+21, y+6);
    else         drawSun(x+21, y+6);
    drawCloud(x+10, y+14);
  } else {
    // Clear
    if (isNight) drawMoon(x+14, y+12);
    else         drawSun(x+14, y+12);
  }
}

void d2Screensaver() {
  if (dispState == 1 || dispState == 3) return;
  oled2.clearDisplay();
  
  oled2.setTextSize(1);
  char ts[9];
  getTimeCStr(ts);
  // Truncate to HH:MM (5 chars) — only when NTP is synced; fallback stays as-is
  if (ntpSynced && strlen(ts) > 5) ts[5] = '\0';
  d2Header("WEATHER", ts);
  d2Divider();
  
  oled2.setTextColor(SSD1306_WHITE);
  if (weatherCode == -1) {
    oled2.setCursor(23, 24);
    oled2.print("Wait..");
  } else {
    // Temperature in left safe zone
    oled2.setTextSize(2);
    oled2.setCursor(23, 24);
    oled2.print((int)weatherTemp);
    oled2.print("C");
    
    // Wind in lower left
    oled2.setTextSize(1);
    oled2.setCursor(23, 44);
    oled2.print("Wnd:");
    oled2.print((int)weatherWind);
    
    // Weather Icon in Right Safe Zone
    struct tm t;
    int currentHour = 12; // default day
    if (getLocalTime(&t)) {
      currentHour = t.tm_hour;
    }
    drawWeatherIcon(&oled2, 80, 24, weatherCode, currentHour);
  }
  oled2.display();
}

void tickIdle() {
  unsigned long now = millis();

  // ── Auto-Sleep: power off OLEDs after sleepTimeoutMs of inactivity ───────
  if (sleepTimeoutMs > 0 && !oledSleeping && (now - idleStartTime >= sleepTimeoutMs)) {
    sleepDisplays();
    return; // nothing to draw — displays are off
  }
  if (oledSleeping) return; // already asleep — skip all drawing

  if (now - drawTimer < DRAW_RATE) return;
  drawTimer = now;

  // Intercept normal rendering if a toast is active
  if (actionToastEnd > 0 && now < actionToastEnd) {
    _drawActionInternal();
    return;
  }

  if (activeLayer == 2 || activeLayer == 6 || activeLayer == 7 || activeLayer == 9) {
    // Do nothing: AppWifi, AppGames, AppSettings and PingMonitor manage their own display.
    // Auto-Sleep still works because it's handled at the top of tickIdle().
  } else if (now - idleStartTime > 15000) {
    d1Screensaver(now);
    d2Screensaver();
  } else {
    if (activeLayer == 3) {
      AppNokia_Draw1();
      AppNokia_Draw2();
    } else {
      d1Draw();   // refresh D1 (handles BLE blink + time)
      d2Idle();   // refresh D2 safe zones
    }
  }
}



// ── STARTUP ANIMATION ─────────────────────────────────────
enum BootTaskState {
    TASK_PENDING,
    TASK_RUNNING,
    TASK_SUCCESS,
    TASK_FAILED
};

volatile int bootPct = 0;
volatile int wifiPct = 0;
volatile int syncPct = 0;
volatile int blePct = 0;

volatile BootTaskState bootState = TASK_PENDING;
volatile BootTaskState wifiState = TASK_PENDING;
volatile BootTaskState syncState = TASK_PENDING;
volatile BootTaskState bleState = TASK_PENDING;

volatile int bootAnimFrame = 0;
unsigned long lastBootAnimTime = 0;

TaskHandle_t bootAnimTaskHandle = NULL;
volatile bool bootTaskRunning = false;

void updateBootScreens() {
  int overallPct = (bootPct + wifiPct + syncPct + blePct) / 4;
  if (overallPct > 100) overallPct = 100;

  // --- Display 1 (Rifat Animation & Master Progress) ---
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  
  int nameLen = 5;
  int namePixelWidth = nameLen * 18;
  
  if (bootAnimFrame <= 5) {
     oled.setTextSize(3);
     char buf[6] = {0};
     strncpy(buf, "Rifat", bootAnimFrame);
     oled.setCursor((SCREEN_W - namePixelWidth) / 2, 18);
     oled.print(buf);
     
     if (bootAnimFrame < 5) {
       int cw = bootAnimFrame * 18;
       oled.fillRect((SCREEN_W - namePixelWidth) / 2 + cw + 1, 18, 10, 22, SSD1306_WHITE);
     }
  } else {
     oled.setTextSize(3);
     oled.setCursor((SCREEN_W - namePixelWidth) / 2, 18);
     if (bootAnimFrame % 2 == 0 || bootAnimFrame == 6) {
       oled.print("Rifat");
     }
  }
  
  oled.drawRect(14, 52, 74, 6, SSD1306_WHITE);
  int fill1 = map(overallPct, 0, 100, 0, 70);
  if (fill1 > 0) oled.fillRect(16, 54, fill1, 2, SSD1306_WHITE);
  
  oled.setTextSize(1);
  char pBuf[8];
  snprintf(pBuf, sizeof(pBuf), "%d%%", overallPct);
  oled.setCursor(94, 51);
  oled.print(pBuf);
  oled.display();

  // --- Display 2 (Task Status) ---
  oled2.clearDisplay();
  oled2.setTextColor(SSD1306_WHITE);
  oled2.setTextSize(1);
  d2Header("SYS:BOOT", "v3.0");
  d2Divider();
  
  const char* taskNames[] = {"BOOT", "WIFI", "SYNC", "BLE"};
  int pcts[] = {bootPct, wifiPct, syncPct, blePct};
  BootTaskState states[] = {bootState, wifiState, syncState, bleState};
  
  for (int i=0; i<4; i++) {
     int y = 18 + (i * 10);
     d2L(y, taskNames[i]);
     
     // Draw short progress bar on the left of the divider (X=47 to 70)
     oled2.drawRect(47, y, 23, 8, SSD1306_WHITE);
     int fill2 = map(pcts[i], 0, 100, 0, 19);
     if (fill2 > 0) oled2.fillRect(49, y+2, fill2, 4, SSD1306_WHITE);
     
     // Draw state indicator on the right of the divider (X=74)
     oled2.setCursor(74, y);
     switch(states[i]) {
       case TASK_PENDING: oled2.print("WAIT"); break;
       case TASK_RUNNING: oled2.print("..."); break;
       case TASK_SUCCESS: oled2.print("OK"); break; // Used to be char 251 (square root/n)
       case TASK_FAILED:  oled2.print("ERR"); break;
     }
  }
  oled2.display();
}

void tickBootAnimation() {
  unsigned long now = millis();
  if (now - lastBootAnimTime > 300) {
     bootAnimFrame = (bootAnimFrame + 1) % 10;
     lastBootAnimTime = now;
  }
}

void bootAnimTask(void *pvParameters) {
  TickType_t lastWakeTime = xTaskGetTickCount();
  while (bootTaskRunning) {
    tickBootAnimation();
    updateBootScreens();
    vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(50));
  }
  
  // Final render to guarantee 100% states are displayed
  updateBootScreens();
  
  bootAnimTaskHandle = NULL;
  vTaskDelete(NULL);
}
// ════════════════════════════════════════════════════════════
// ════════════════════════════════════════════════════════════
// ── BOOT MENU ─────────────────────────────────────────────
void oledDrawIcon(int16_t x, int16_t y, const unsigned char* bitmap, int16_t w, int16_t h) {
  int16_t byteWidth = (w + 7) / 8;
  for (int16_t j = 0; j < h; j++) {
    for (int16_t i = 0; i < w; i++) {
      uint8_t b = pgm_read_byte(&bitmap[j * byteWidth + i / 8]);
      if (b & (1 << (i & 7))) {
        oled.drawPixel(x + i, y + j, SSD1306_WHITE);
      }
    }
  }
}

void drawMenuList() {
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);

  // Determine viewport: show 5 items at a time
  if (menuSel < menuScroll) menuScroll = menuSel;
  if (menuSel >= menuScroll + 5) menuScroll = menuSel - 4;

  int startIdx = menuScroll;
  int endIdx = min(MENU_COUNT, startIdx + 5);

  for (int i = startIdx; i < endIdx; i++) {
    int y = (i - startIdx) * 12 + 2; // 12px row height, 2px top padding
    
    if (i == menuSel) {
      // Inverted highlight
      oled.fillRect(0, y - 2, 128, 12, SSD1306_WHITE);
      oled.setTextColor(SSD1306_BLACK, SSD1306_WHITE); // Black text on white background
    } else {
      oled.setTextColor(SSD1306_WHITE, SSD1306_BLACK); // Normal text
    }

    oled.setCursor(4, y);
    oled.print(i + 1);
    oled.print(". ");
    oled.print(menuNames[i]);
  }
  
  // Revert text color back for other draw calls
  oled.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
  oled.display();

  // D2 text rendering (Safe Zones)
  oled2.clearDisplay();
  oled2.setTextColor(SSD1306_WHITE);
  oled2.setTextSize(1);
  
  char ts[9]; getTimeCStr(ts);
  d2Header(ts, "MENU");
  d2Divider();
  
  // Layer name line 1
  char lName[16];
  snprintf(lName, sizeof(lName), "LAYER %02d", menuLayers[menuSel]);
  d2L(20, lName);
  
  // App name line 2/3
  const char* name = menuNames[menuSel];
  const char* space = strchr(name, ' ');
  char line1[16] = {0};
  char line2[16] = {0};
  
  if (space != nullptr && (space - name) <= 8) {
    int len1 = space - name;
    strncpy(line1, name, len1);
    strncpy(line2, space + 1, 15);
    d2L(32, line1);
    d2L(44, line2);
  } else {
    strncpy(line1, name, 8);
    d2L(32, line1);
    if (strlen(name) > 8) {
      strncpy(line2, name + 8, 15);
      d2L(44, line2);
    }
  }

  // Right side blinking indicator inside safe zone
  if ((millis() / 500) % 2 == 0) {
    oled2.fillRect(78, 24, 12, 12, SSD1306_WHITE);
  }
  
  oled2.display();
}

void updateBootMenu() {
  unsigned long now = millis();

  // If sleep timeout reached, sleep displays
  if (sleepTimeoutMs > 0 && !oledSleeping && (now - idleStartTime >= sleepTimeoutMs)) {
    sleepDisplays();
  }
  if (oledSleeping) return; // Completely asleep, ignore drawing

  // Draw either Screensaver or Menu
  if (now - idleStartTime > 15000) {
    d1Screensaver(now);
    d2Screensaver();
  } else {
    drawMenuList();
  }

  // Input Handling is now event-driven and routed through AppManager_HandleEvent -> BootMenu_HandleInput
}

void BootMenu_HandleInput(LogicalEvent ev) {
  if (ev == EV_UP_TAP || ev == EV_UP_HOLD) {
    beepTap();
    menuSel = (menuSel - 1 + MENU_COUNT) % MENU_COUNT;
    resetIdle();
  } else if (ev == EV_DOWN_TAP || ev == EV_DOWN_HOLD) {
    beepTap();
    menuSel = (menuSel + 1) % MENU_COUNT;
    resetIdle();
  } else if (ev == EV_CENTER_TAP) {
    beepDone();
    AppManager_SwitchApp(menuLayers[menuSel]);
    
    // Force clear screens so normal apps redraw cleanly
    oled.clearDisplay(); oled.display();
    oled2.clearDisplay(); oled2.display();
    
    resetIdle();
    // No need to wait for physical release because EV_CENTER_TAP only fires upon release!
  } else if (ev == EV_LEFT_TAP || ev == EV_RIGHT_TAP || ev == EV_LEFT_HOLD || ev == EV_RIGHT_HOLD) {
    // LEFT/RIGHT wake up screensaver but do nothing else in vertical menu.
    resetIdle();
  } else if (ev == EV_CENTER_HOLD_5S) {
    dispState = 0;          // Force both displays ON
    setDisplays();
    wakeDisplays();
    drawAction(">> bad apple!");
    delay(300);
    extern void playBadApple(Adafruit_SSD1306*, Adafruit_SSD1306*);
    playBadApple(&oled, &oled2);
    resetIdle();
  }
}

// ── Wi-Fi credential accessors (declared in globals.h) ─────────────────────
// Thin wrappers so other translation units can read WIFI_NETS[] safely.
int GetWifiNetCount() { return WIFI_NET_COUNT; }
const char* GetWifiSSID(int i) {
  return (i >= 0 && i < WIFI_NET_COUNT) ? WIFI_NETS[i].ssid : "";
}
const char* GetWifiPass(int i) {
  return (i >= 0 && i < WIFI_NET_COUNT) ? WIFI_NETS[i].pass : "";
}

// ════════════════════════════════════════════════════════════
void WaitAllKeysReleased() {
  while (true) {
    bool anyPressed = false;
    // Check main directional buttons
    if (digitalRead(PIN1) == LOW || digitalRead(PIN2) == LOW ||
        digitalRead(PIN3) == LOW || digitalRead(PIN4) == LOW ||
        digitalRead(PIN5) == LOW) {
      anyPressed = true;
    }
    
    // Check Matrix Keypad
    matrixPad.getKeys(); // Update internal state
    for (int i = 0; i < LIST_MAX; i++) {
      if (matrixPad.key[i].kstate == PRESSED || matrixPad.key[i].kstate == HOLD) {
        anyPressed = true;
      }
    }
    
    if (!anyPressed) break;
    delay(10); // spin until fully released
  }
}
// ════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  randomSeed(analogRead(0) ^ analogRead(1) ^ analogRead(2) ^ millis());
  fnIdx = random(0, NAME_COUNT);
  lnIdx = random(0, NAME_COUNT);
  genPwd();

  // 1. BOOT TASK
  bootState = TASK_RUNNING;
  bootPct = 0;

  // Initialize Black/White (oled, D32/D33)
  I2C_2.begin(SDA_2, SCL_2, 400000);
  if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED 1 failed");
  } else { oled.clearDisplay(); oled.display(); }

  // Initialize Blue/Yellow (oled2, D21/D22)
  Wire.begin(); Wire.setClock(400000);
  if (!oled2.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED 2 failed");
  } else { oled2.clearDisplay(); oled2.display(); }

  bootTaskRunning = true;
  xTaskCreatePinnedToCore(
    bootAnimTask,
    "BootAnim",
    4096,
    NULL,
    1,
    &bootAnimTaskHandle,
    0
  );

  pinMode(PIN1, INPUT_PULLUP);
  pinMode(PIN2, INPUT_PULLUP);
  pinMode(PIN3, INPUT_PULLUP);
  pinMode(PIN4, INPUT_PULLUP);
  pinMode(PIN5, INPUT_PULLUP);
  pinMode(BUZZ, OUTPUT);

  bootPct = 100;
  bootState = TASK_SUCCESS;


  // 2. WIFI TASK
  wifiState = TASK_RUNNING;
  wifiPct = 0;


  // Initialize Preferences and load WiFi configs
  prefs.begin("macro-kb", false);
  silentMode = prefs.getBool("silentMode", false);
  sleepTimeoutMs = prefs.getULong("sleepTimeout", 5UL * 60UL * 1000UL);
  
  WIFI_NET_COUNT = prefs.getInt("wifi_cnt", 0);
  
  // Migration: If no networks exist, populate with the legacy hardcoded credentials
  if (WIFI_NET_COUNT == 0) {
    const char* legacy_ssids[] = {"Faysal", "Raha", "Tanvir Ahmed Rifat", "D LAB", "UCEP_AUTO"};
    const char* legacy_passes[] = {"Rifat007", "rafsan25631", "Rifat#007", "ent@1981#", "auto_!@ukwf#$524"};
    for (int i = 0; i < 5; i++) {
        prefs.putString(("ssid_" + String(i)).c_str(), legacy_ssids[i]);
        prefs.putString(("pass_" + String(i)).c_str(), legacy_passes[i]);
    }
    prefs.putInt("wifi_cnt", 5);
    WIFI_NET_COUNT = 5;
  }

  for (int i = 0; i < WIFI_NET_COUNT && i < 10; i++) {
    String s = prefs.getString(("ssid_" + String(i)).c_str(), "");
    String p = prefs.getString(("pass_" + String(i)).c_str(), "");
    strncpy(WIFI_NETS[i].ssid, s.c_str(), 32); WIFI_NETS[i].ssid[32] = '\0';
    strncpy(WIFI_NETS[i].pass, p.c_str(), 64); WIFI_NETS[i].pass[64] = '\0';
  }
  prefs.end(); // !! Close so web-server task Preferences handles can open cleanly

  wifiStartMs = millis();
  unsigned long wifiAttemptStart = millis();
  bool wifiConnected = false;

  if (WIFI_NET_COUNT > 0) {
    WiFi.mode(WIFI_STA);   // Must be set BEFORE WiFi.begin() on ESP32
    WiFi.setAutoReconnect(false);
    WiFi.setSleep(false);  // Disable power-save — improves NTP reliability
    delay(100);

    WIFI_SSID = WIFI_NETS[0].ssid;
    Serial.printf("[WiFi] Trying: %s\n", WIFI_NETS[0].ssid);
    WiFi.begin(WIFI_NETS[0].ssid, WIFI_NETS[0].pass);

    // Try primary
    while (millis() - wifiAttemptStart < 12000) {
      if (WiFi.status() == WL_CONNECTED) {
         wifiConnected = true;
         break;
      }

      wifiPct = 10 + ((millis() - wifiAttemptStart) / 150) % 80;
      delay(10);
    }

    // Try fallbacks
    if (!wifiConnected) {
      for (int ni = 1; ni < WIFI_NET_COUNT; ni++) {
        WiFi.disconnect(true);
        delay(200);
        WIFI_SSID = WIFI_NETS[ni].ssid;
        Serial.printf("[WiFi] Trying fallback: %s\n", WIFI_NETS[ni].ssid);
        WiFi.begin(WIFI_NETS[ni].ssid, WIFI_NETS[ni].pass);
        
        unsigned long tryStart = millis();
        while (millis() - tryStart < 8000) {
          if (WiFi.status() == WL_CONNECTED) {
            wifiConnected = true;
            break;
          }
          wifiPct = 10 + ((millis() - tryStart) / 100) % 80;
          delay(10);
        }
        if (wifiConnected) break;
      }
    }
  }

  if (wifiConnected) {
    delay(200); // Let DHCP/routing fully stabilise before NTP
    Serial.printf("[WiFi] Connected: %s  IP: %s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
  }

  if (!wifiConnected) {
    // Start SoftAP for Setup
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP("Macro-Keyboard", "");
    WIFI_SSID = "Macro-Keyboard (AP)";
  }

  if (wifiConnected) {
    wifiPct = 100;
    wifiState = TASK_SUCCESS;
  } else {
    wifiPct = 100;
    wifiState = TASK_FAILED;
  }


  // 3. SYNC TASK — NTP (WiFi still connected here)
  if (wifiConnected) {
    syncState = TASK_RUNNING;
    syncPct = 0;

    Serial.printf("[NTP] Starting sync (3 servers, GMT+%ld)\n", GMT_OFFSET_S / 3600);
    // Pass 3 servers — ESP32 SNTP tries all simultaneously, uses fastest reply
    configTime(GMT_OFFSET_S, DST_OFFSET_S,
               "pool.ntp.org",
               "time.google.com",
               "time.cloudflare.com");
    
    unsigned long waitStart = millis();
    while (!ntpSynced && (millis() - waitStart < 15000)) { // 15s for mobile hotspots
      struct tm timeinfo;
      if (getLocalTime(&timeinfo, 1000)) {
         ntpSynced = true;
         char tsBuf[32];
         strftime(tsBuf, sizeof(tsBuf), "%Y-%m-%d %H:%M:%S", &timeinfo);
         Serial.printf("[NTP] Synced: %s\n", tsBuf);
      }
      syncPct = 10 + ((millis() - waitStart) / 188) % 80;
    }
    
    if (ntpSynced) {
      syncPct = 100;
      syncState = TASK_SUCCESS;
      fetchWeatherOnce();
    } else {
      Serial.println("[NTP] UDP SYNC FAILED — trying HTTP fallback...");
      syncPct = 50;
      
      HTTPClient http;
      http.begin("http://google.com/");
      const char * headerKeys[] = {"Date"};
      http.collectHeaders(headerKeys, 1);
      http.setTimeout(4000);
      int code = http.sendRequest("HEAD");
      
      if (code > 0 && http.hasHeader("Date")) {
        String dateStr = http.header("Date");
        Serial.printf("[HTTP] Got Date: %s\n", dateStr.c_str());
        
        struct tm tm = {0};
        // Date header format: Fri, 18 Sep 2026 16:09:27 GMT
        if (strptime(dateStr.c_str(), "%a, %d %b %Y %H:%M:%S %Z", &tm) != NULL) {
          // mktime uses the local timezone environment variable.
          // To ensure we get the true UTC epoch from the GMT string,
          // we force TZ to UTC before parsing, then restore it.
          setenv("TZ", "UTC0", 1);
          tzset();
          time_t t = mktime(&tm);
          
          struct timeval tv = { .tv_sec = t, .tv_usec = 0 };
          settimeofday(&tv, NULL);
          ntpSynced = true;
          
          // Re-apply the GMT+6 offset for Khulna, Bangladesh
          configTime(GMT_OFFSET_S, DST_OFFSET_S,
                     "pool.ntp.org", "time.google.com", "time.cloudflare.com");
          Serial.println("[HTTP] Time synced successfully");
        }
      }
      http.end();

      syncPct = 100;
      if (ntpSynced) {
        syncState = TASK_SUCCESS;
        fetchWeatherOnce();
      } else {
        syncState = TASK_FAILED;
        Serial.println("[SYNC] All time sync methods failed");
      }
    }
  } else {
    syncState = TASK_FAILED;
    syncPct = 100;
    Serial.println("[NTP] Skipped — no WiFi");
  }


  // 4. BLE TASK — shut down WiFi radio first to free radio for BLE
  bleState = TASK_RUNNING;
  blePct = 0;

  WiFi.disconnect(true);
  blePct = 25;
  delay(100);
  WiFi.mode(WIFI_OFF);
  blePct = 50;
  delay(100);

  // NimBLE init
  ble.setSecurityMode(HIDSecurity::JustWorks);
  blePct = 75;
  
  ble.setRandomAddress(false);
  ble.setLogLevel(HIDLogLevel::Normal);

  ble.begin(); // BLE starts with 100% radio
  blePct = 100;
  bleState = TASK_SUCCESS;
  
  // FINAL TRANSITION
  bootTaskRunning = false;
  while(bootAnimTaskHandle != NULL) {
    delay(10);
  }
  
  buzzNote(1047, 100); delay(130);
  buzzNote(1319, 100); delay(130);
  buzzNote(1568, 100); delay(130);
  buzzNote(2093, 200);
}
// ════════════════════════════════════════════════════════════
void loop() {
  char mKey;
  #define MDELAY(x) if(macroDelay(x)) { ble.releaseAll(); resetIdle(); goto ABORT_MACRO; }
  macroAborted = false;
  macroStartMs = millis();

  AppManager_Update();

  mKey = matrixPad.getKey();
  if (mKey) {
    AppManager_HandleMatrix(mKey);
  }

  // ── Universal Return-to-Main-Menu (Hold * for 2s) ──
  static unsigned long starHoldStart = 0;
  static bool starIsHeld = false;
  static bool starHoldTriggered = false;

  bool starPressed = false;
  if (matrixPad.getKeys()) {
    // Allows us to inspect the raw state array without consuming events
  }
  
  for (int i=0; i<LIST_MAX; i++) {
    if (matrixPad.key[i].kchar == '*' && (matrixPad.key[i].kstate == PRESSED || matrixPad.key[i].kstate == HOLD)) {
      starPressed = true;
    }
  }

  if (starPressed) {
    if (!starIsHeld) {
      starIsHeld = true;
      starHoldStart = millis();
      starHoldTriggered = false;
    } else if (!starHoldTriggered && (millis() - starHoldStart >= 2000)) {
      starHoldTriggered = true;
      AppManager_HandleEvent(EV_BACKSPACE_HOLD_2S);
    }
  } else {
    starIsHeld = false;
  }

  // ── Universal Silent Toggle (Hold # for 2s) ──
  static unsigned long hashHoldStart = 0;
  static bool hashIsHeld = false;
  static bool hashHoldTriggered = false;

  bool hashPressed = false;
  for (int i=0; i<LIST_MAX; i++) {
    if (matrixPad.key[i].kchar == '#' && (matrixPad.key[i].kstate == PRESSED || matrixPad.key[i].kstate == HOLD)) {
      hashPressed = true;
    }
  }

  if (hashPressed) {
    if (!hashIsHeld) {
      hashIsHeld = true;
      hashHoldStart = millis();
      hashHoldTriggered = false;
    } else if (!hashHoldTriggered && (millis() - hashHoldStart >= 2000)) {
      hashHoldTriggered = true;
      silentMode = !silentMode;
      // Single short beep if we just turned silent mode OFF
      if (!silentMode) {
        buzzNote(1800, 50);
      }
    }
  } else {
    hashIsHeld = false;
  }


  bool con = ble.isConnected();

  // ╔═══════════════════════════════════════════════════╗
  // ║ OLED WAKE-ON-BUTTON (Burn-in Protection)          ║
  // ╚═══════════════════════════════════════════════════╝
  // If displays are sleeping and any button is pressed, wake them up.
  // The button press is fully consumed by the wake action — it does NOT
  // trigger a macro, so users don't accidentally type something in the dark.
  if (oledSleeping) {
    bool anyPressed = (digitalRead(PIN1) == LOW || digitalRead(PIN2) == LOW ||
                       digitalRead(PIN3) == LOW || digitalRead(PIN4) == LOW ||
                       digitalRead(PIN5) == LOW);
    
    // Also check if any matrix key is pressed
    if (matrixPad.getKeys()) {
      for (int i=0; i<LIST_MAX; i++) {
        if (matrixPad.key[i].stateChanged && matrixPad.key[i].kstate == PRESSED) {
          anyPressed = true;
        }
      }
    }

    if (anyPressed) {
      wakeDisplays();
      resetIdle();
      // Wait for full release of the main buttons so this press doesn't bleed into a macro
      while (digitalRead(PIN1) == LOW || digitalRead(PIN2) == LOW ||
             digitalRead(PIN3) == LOW || digitalRead(PIN4) == LOW ||
             digitalRead(PIN5) == LOW) { delay(10); }
      // Wait for matrix keypad release
      while(matrixPad.getKeys() && matrixPad.isPressed(matrixPad.key[0].kchar)) { delay(10); }
      delay(100); // debounce
    } else {
      tickIdle(); // still call tickIdle so it can check sleep timer (no-op while sleeping)
    }
    delay(50);  // Yield to IDLE task to massively reduce CPU power while OLEDs are off
    return; // skip all macro logic while sleeping / just woken
  }

  // ║ WIFI ANALYZER MODE OVERRIDE DELETED               ║

  // ║ GAMES MODE OVERRIDE DELETED                       ║
  // ── BACKGROUND BLE BEEP ───────────────────────────────────
  if (con != wasCon) {
    wasCon = con;
    resetIdle();
    if (con) beepConnect(); else beepDisconnect();
  }

  // NTP was fully resolved during setup() before WiFi was shut down.
  // No background NTP loop needed — ntpSynced is already set correctly.

  // ── Input Manager ──
  LogicalEvent ev = SystemInput_Update();
  if (ev != EV_NONE) {
    AppManager_HandleEvent(ev);
  }

  // ── BOOT MENU OVERRIDE ────────────────────────────────────
  if (inBootMenu) {
    updateBootMenu();
    return;
  }

  if (macroAborted) goto ABORT_MACRO;

  tickIdle(); // MUST render displays and manage sleep timer
  return; // DO NOT fall through to ABORT_MACRO

  ABORT_MACRO:
  // Reset all button tracking state so no stale hold is remembered
  SystemInput_ResetState();
  

  // ── CRITICAL: wait until every button is physically released ─────────────
  // Without this, the button that triggered the abort is still LOW when
  // loop() restarts, causing it to immediately fire a NEW macro.
  playBeep(7); // error tone: clear audio feedback that macro was stopped
  WaitAllKeysReleased();
  delay(150); // short debounce after release
  return;
}
