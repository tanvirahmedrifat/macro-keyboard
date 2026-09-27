#include "app_media.h"

// FIX 12: Show media button map on OLED 2 so user doesn't need to memorize layout.
static void drawMediaLayout() {
    if (!oled2Active) return;
    oled2.clearDisplay();
    oled2.setTextColor(SSD1306_WHITE);
    oled2.setTextSize(1);
    d2Header("MEDIA", "CTL");
    d2Divider();
    // Left zone: button functions (size 1)
    d2L(18, "U:VolDn");
    d2L(28, "D:VolUp");
    d2L(38, "L:Prev");
    d2L(48, "R:Next");
    // Right zone: hold functions
    d2R(18, "Mute");
    d2R(28, "Play");
    d2R(38, "Rew");
    d2R(48, "FF");
    oled2.display();
}

#define MDELAY(x) if(macroDelay(x)) { ble.releaseAll(); resetIdle(); return; }

void AppMedia_Btn1_Hold() {
  drawAction(">> mute");
  drawMediaLayout(); // FIX 12: keep OLED 2 updated
  ble.tap(MEDIA_MUTE);
  MDELAY(400);
}

void AppMedia_Btn1_Tap() {
  drawAction(">> vol down");
  drawMediaLayout();
  ble.tap(MEDIA_VOLUME_DOWN);
  MDELAY(200);
}


void AppMedia_Btn2_Hold() {
  drawAction(">> play/pause");
  drawMediaLayout();
  ble.tap(MEDIA_PLAY_PAUSE);
  MDELAY(400);
}

void AppMedia_Btn2_Tap() {
  drawAction(">> vol up");
  drawMediaLayout();
  ble.tap(MEDIA_VOLUME_UP);
  MDELAY(200);
}


void AppMedia_Btn3_Hold() {
  drawAction(">> rewind");
  drawMediaLayout();
  ble.tap(MEDIA_REWIND);
  MDELAY(400);
}

void AppMedia_Btn3_Tap() {
  drawAction(">> prev track");
  drawMediaLayout();
  ble.tap(MEDIA_PREV_TRACK);
  MDELAY(200);
}

void AppMedia_Btn4_Hold() {
  drawAction(">> fast forward");
  drawMediaLayout();
  ble.tap(MEDIA_FAST_FORWARD);
  MDELAY(400);
}

void AppMedia_Btn4_Tap() {
  drawAction(">> next track");
  drawMediaLayout();
  ble.tap(MEDIA_NEXT_TRACK);
  MDELAY(200);
}

void AppMedia_Btn5_Tap() {
  drawAction(">> play/pause");
  drawMediaLayout();
  ble.tap(MEDIA_PLAY_PAUSE);
  MDELAY(200);
}

