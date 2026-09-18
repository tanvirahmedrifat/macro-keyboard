#include "app_games.h"
#include "game_snake.h"
#include "game_pong.h"
#include "game_tetris.h"

// Sub-menu states
#define SUBMENU_IDLE 0
#define GAME_SNAKE   1
#define GAME_PONG    2
#define GAME_TETRIS  3

static int currentGame = SUBMENU_IDLE;
static int menuSelection = 0;
static const int numGames = 3;
static const char* gameNames[] = {
    "Snake",
    "Pong",
    "Tetris"
};
static bool menuRedraw = true;

void AppGames_Init() {
    // Radios are managed by AppContainer system now

    currentGame = SUBMENU_IDLE;
    menuSelection = 0;
    menuRedraw = true;
}

void AppGames_ExitToMenu() {
    currentGame = SUBMENU_IDLE;
    menuRedraw = true;
}

void AppGames_Update() {
    if (currentGame == SUBMENU_IDLE) {
        if (menuRedraw) {
            oled.clearDisplay();
            oled.setTextSize(1);
            oled.setTextColor(SSD1306_WHITE);
            oled.setCursor(0, 0);
            oled.println("-- GAMES MENU --");
            
            for (int i = 0; i < numGames; i++) {
                if (i == menuSelection) {
                    oled.setTextColor(SSD1306_BLACK, SSD1306_WHITE); // Inverted for selected
                } else {
                    oled.setTextColor(SSD1306_WHITE);
                }
                oled.setCursor(10, 20 + i * 12);
                oled.print(gameNames[i]);
            }
            oled.display();
            
            // Draw status display for Games Menu
            oled2.clearDisplay();
            oled2.setTextColor(SSD1306_WHITE);
            oled2.setTextSize(1);
            oled2.setCursor(0, 10);
            oled2.print("Select Game");
            oled2.setCursor(0, 25);
            oled2.print("Hold * Exit");
            oled2.display();
            
            menuRedraw = false;
        }
    } else if (currentGame == GAME_SNAKE) {
        GameSnake_Update();
    } else if (currentGame == GAME_PONG) { 
        GamePong_Update(); 
    } else if (currentGame == GAME_TETRIS) { 
        GameTetris_Update(); 
    }
}

void AppGames_HandleInput(LogicalEvent ev) {
    if (currentGame == SUBMENU_IDLE) {
        if (ev == EV_UP_TAP || ev == EV_UP_HOLD) {
            menuSelection--;
            if (menuSelection < 0) menuSelection = numGames - 1;
            menuRedraw = true;
            beepTap();
        } else if (ev == EV_DOWN_TAP || ev == EV_DOWN_HOLD) {
            menuSelection++;
            if (menuSelection >= numGames) menuSelection = 0;
            menuRedraw = true;
            beepTap();
        } else if (ev == EV_CENTER_TAP) {
            beepTap();
            if (menuSelection == 0) {
                currentGame = GAME_SNAKE;
                GameSnake_Init();
            } else if (menuSelection == 1) {
                currentGame = GAME_PONG;
                GamePong_Init();
            } else if (menuSelection == 2) {
                currentGame = GAME_TETRIS;
                GameTetris_Init();
            }
        }
    } else if (currentGame == GAME_SNAKE) {
        GameSnake_HandleInput(ev);
    } else if (currentGame == GAME_PONG) { 
        GamePong_HandleInput(ev); 
    } else if (currentGame == GAME_TETRIS) { 
        GameTetris_HandleInput(ev); 
    }
}
