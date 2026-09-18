#include "app_media.h"

#define MDELAY(x) if(macroDelay(x)) { ble.releaseAll(); resetIdle(); return; }

void AppMedia_Btn1_Hold() {
  drawAction(">> mute");
  ble.tap(MEDIA_MUTE);
  MDELAY(400);
}

void AppMedia_Btn1_Tap() {
  drawAction(">> vol down");
  ble.tap(MEDIA_VOLUME_DOWN);
  MDELAY(200);
}

void AppMedia_Btn2_Hold() {
  drawAction(">> unmute");
  ble.tap(MEDIA_MUTE);
  MDELAY(400);
}

void AppMedia_Btn2_Tap() {
  drawAction(">> vol up");
  ble.tap(MEDIA_VOLUME_UP);
  MDELAY(200);
}

void AppMedia_Btn3_Hold() {
  drawAction(">> rewind");
  ble.tap(MEDIA_REWIND);
  MDELAY(400);
}

void AppMedia_Btn3_Tap() {
  drawAction(">> prev track");
  ble.tap(MEDIA_PREV_TRACK);
  MDELAY(200);
}

void AppMedia_Btn4_Hold() {
  drawAction(">> next track");
  ble.tap(MEDIA_FAST_FORWARD);
  MDELAY(400);
}

void AppMedia_Btn4_Tap() {
  drawAction(">> play/pause");
  ble.tap(MEDIA_PLAY_PAUSE);
  MDELAY(200);
}
