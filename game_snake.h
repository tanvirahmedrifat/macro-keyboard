#ifndef GAME_SNAKE_H
#define GAME_SNAKE_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "system_input.h"

#define DIR_RIGHT 0
#define DIR_UP    1
#define DIR_LEFT  2
#define DIR_DOWN  3

extern Adafruit_SSD1306 oled;
extern Adafruit_SSD1306 oled2;

void GameSnake_Init();
void GameSnake_Update();
void GameSnake_HandleInput(LogicalEvent ev);

extern void beepTap();
extern void beepDone();
extern void playBeep(int count);

#endif
