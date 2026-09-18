#pragma once
#include <Arduino.h>
#include <Adafruit_SSD1306.h>
#include "bad_apple_data.h"
#include "heatshrink_decoder.h"

// Allocate memory for the decoder (20 byte header + 4096 byte flexible array)
static uint8_t hsd_mem[sizeof(heatshrink_decoder) + (1 << 11) + 2048];
static heatshrink_decoder& hsd = *(heatshrink_decoder*)hsd_mem;
static int16_t curr_x = 0;
static int16_t curr_y = 0;
static int32_t runlength = -1;
static int32_t c_to_dup = -1;
static uint32_t lastRefresh = 0;

static Adafruit_SSD1306* d1 = nullptr;
static Adafruit_SSD1306* d2 = nullptr;

// Inline button poll — any LOW pin = abort
static inline bool anyBtnPressed() {
  return (digitalRead(12) == LOW || digitalRead(13) == LOW ||
          digitalRead(14) == LOW || digitalRead(26) == LOW ||
          digitalRead(27) == LOW);
}

static void putPixels(uint8_t c, int32_t len, bool& abortFlag) {
  uint8_t b = 0;
  while(len--) {
    if (abortFlag) return;
    b = 128;
    for(int i=0; i<8; i++) {
      int color = (c & b) ? SSD1306_WHITE : SSD1306_BLACK;
      if (d1) d1->drawPixel(curr_x, curr_y, color);
      if (d2) d2->drawPixel(curr_x, curr_y, color);
      b >>= 1;
      curr_x++;

      if(curr_x >= 128) {
        curr_x = 0;
        curr_y++;

        // Check buttons at every row boundary (every 128 pixels).
        // This gives 64 checks per frame during pixel drawing.
        if (anyBtnPressed()) { abortFlag = true; return; }

        if(curr_y >= 64) {
          curr_y = 0;

          // Check before display calls
          if (anyBtnPressed()) { abortFlag = true; return; }

          if (d1) d1->display();
          if (d2) d2->display();

          // Check after display calls.
          // CRITICAL FIX: Both display() calls together take >33ms over I2C,
          // meaning the frame-gap while-loop below was entered with elapsed>=33
          // and its body NEVER executed. This check was the only one that never
          // ran, making the abort completely unresponsive during playback.
          if (anyBtnPressed()) { abortFlag = true; return; }

          // Frame-rate limiter: pad remaining time up to 33ms (30 fps)
          while((millis() - lastRefresh) < 33) {
            if (anyBtnPressed()) { abortFlag = true; return; }
            delay(1);
          }
          delay(1); // Unconditionally feed the FreeRTOS Watchdog to prevent crash
          lastRefresh = millis();
        }
      }
    }
  }
}

static void decodeRLE(uint8_t c, bool& abortFlag) {
  if(c_to_dup == -1) {
    if((c == 0x55) || (c == 0xaa)) {
      c_to_dup = c;
    } else {
      putPixels(c, 1, abortFlag);
    }
  } else {
    if(runlength == -1) {
      if(c == 0) {
        putPixels(c_to_dup & 0xff, 1, abortFlag);
        c_to_dup = -1;
      } else if((c & 0x80) == 0) {
        if(c_to_dup == 0x55) {
          putPixels(0, c, abortFlag);
        } else {
          putPixels(255, c, abortFlag);
        }
        c_to_dup = -1;
      } else {
        runlength = c & 0x7f;
      }
    } else {
      runlength = runlength | (c << 7);
      if(c_to_dup == 0x55) {
        putPixels(0, runlength, abortFlag);
      } else {
        putPixels(255, runlength, abortFlag);
      }
      c_to_dup = -1;
      runlength = -1;
    }
  }
}

#define RLEBUFSIZE 4096

void playBadApple(Adafruit_SSD1306* disp1, Adafruit_SSD1306* disp2) {
  d1 = disp1;
  d2 = disp2;
  bool abortFlag = false;

  // Wait for the user to release the button that triggered this
  // before starting, otherwise it will instantly abort!
  while (anyBtnPressed()) { delay(10); }

  if (d1) { d1->clearDisplay(); d1->display(); }
  if (d2) { d2->clearDisplay(); d2->display(); }

  curr_x = 0;
  curr_y = 0;
  runlength = -1;
  c_to_dup = -1;
  lastRefresh = millis();

  heatshrink_decoder_reset(&hsd);
  size_t count = 0;
  uint32_t sunk = 0;

  // Static buffers to avoid blowing the 8KB FreeRTOS stack
  static uint8_t rle_buf[RLEBUFSIZE];
  static uint8_t compbuf[2048];

  const uint32_t filesize = bad_apple_len;
  uint32_t srcHead = 0;

  while(srcHead < filesize) {
    if (abortFlag) break;

    size_t toSink = filesize - srcHead;
    if (toSink > 2048) toSink = 2048;

    memcpy_P(compbuf, &bad_apple_video[srcHead], toSink);

    size_t sinkHead = 0;
    while(toSink > 0) {
      if (abortFlag) break;
      HSD_sink_res sres = heatshrink_decoder_sink(&hsd, &compbuf[sinkHead], toSink, &count);
      toSink  -= count;
      sinkHead += count;
      sunk     += count;

      if (sunk == filesize) heatshrink_decoder_finish(&hsd);

      HSD_poll_res pres;
      do {
        if (abortFlag) break;
        rle_buf[0] = 0;
        size_t rle_size = 0;
        pres = heatshrink_decoder_poll(&hsd, rle_buf, RLEBUFSIZE, &rle_size);
        if(pres < 0) { abortFlag = true; break; }

        size_t rle_bufhead = 0;
        while(rle_size--) {
          if (abortFlag) break;
          decodeRLE(rle_buf[rle_bufhead++], abortFlag);
        }
      } while (pres == HSDR_POLL_MORE && !abortFlag);
    }
    srcHead += sinkHead;
  }

  // Clear and wait for full button release
  if (d1) { d1->clearDisplay(); d1->display(); }
  if (d2) { d2->clearDisplay(); d2->display(); }
  while (anyBtnPressed()) { delay(10); }
}
