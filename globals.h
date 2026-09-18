#ifndef GLOBALS_H
#define GLOBALS_H

#include <HijelHID_BLEKeyboard.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>

struct WifiCred { char ssid[33]; char pass[65]; };
extern WifiCred WIFI_NETS[10];
extern int WIFI_NET_COUNT;

#define SCREEN_W   128
#define SCREEN_H    64

// Shared Display Instances
extern Adafruit_SSD1306 oled;
extern Adafruit_SSD1306 oled2;

// BLE Instance
extern HijelHID_BLEKeyboard ble;

// Hardware Pins
extern const int PIN1;
extern const int PIN2;
extern const int PIN3;
extern const int PIN4;
extern const int PIN5;
extern const int BUZZ;

// Global State
extern int activeLayer;
extern bool inBootMenu;
extern bool ntpSynced;
extern bool oledSleeping;
extern bool silentMode;
extern unsigned long sleepTimeoutMs;

// Display Utilities
void wakeDisplays();
void resetIdle();
void buzzNote(int freq, int duration);
void beepTap();
void beepDone();
void WaitAllKeysReleased();
void centered(const char* txt, int y, int size = 1);
void d2Divider();
void d2Header(const char* left, const char* right);
void d2L(int y, const char* text);
void d2R(int y, const char* text);
void getTimeCStr(char* buf);

extern int dispState;

// Macro Helpers
bool macroDelay(int ms);
void humanType(const char* text);
void humanTap(uint8_t key);
void drawAction(const char* msg);
void drawPasswordTyping();

extern bool d2HoldPwd;
extern char pwd[];
extern const int PWD_LEN;
extern int fnIdx;
extern int lnIdx;
String firstName();
String lastName();
void newProfile();
void beepHoldReady();
void beepError();
void beepNewProfile();


void playBadApple(Adafruit_SSD1306* d1, Adafruit_SSD1306* d2);
void setDisplays();

// ── Wi-Fi Credential Accessors ─────────────────────────────────────────────
// Thin wrappers around WIFI_NETS[] defined in main sketch.
// Allows other translation units to read credentials without a shared struct.
int         GetWifiNetCount();
const char* GetWifiSSID(int index);
const char* GetWifiPass(int index);

#endif
