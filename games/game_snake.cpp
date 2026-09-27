#include "game_snake.h"

// Define states
#define STATE_WELCOME    0
#define STATE_DIFFICULTY 1
#define STATE_PLAYING    2
#define STATE_GAMEOVER   3

// Grid configuration
// Logical grid is 32x16.
static const int GRID_X_MAX = 31;
static const int GRID_Y_MAX = 15;
static const int MAX_SNAKE_LEN = 100;

static int snake_x[MAX_SNAKE_LEN];
static int snake_y[MAX_SNAKE_LEN];
static int snake_length = 3;
static int food_x;
static int food_y;

static int current_direction = DIR_RIGHT;
static int next_direction = DIR_RIGHT;
static bool input_locked = false;

static unsigned long lastMoveTime = 0;
static unsigned int base_speed = 130;
// FIX: Store the difficulty-selected base speed separately.
// base_speed is modified by the speed-scaling system as the snake grows.
// diff_base_speed holds the original speed for the chosen difficulty so
// resetGame() can restore it correctly when the player retries.
static unsigned int diff_base_speed = 130;
static int diff_sel = 1; // 0=Easy, 1=Med, 2=Hard
static int gameState = STATE_WELCOME;
static bool redrawNeeded = true;
static bool needsStatusRedraw = true;

// Helper to spawn food
static void spawnFood() {
    bool valid = false;
    // BUG-29 FIX: Added bail-out counter. If the grid is nearly full (e.g. snake
    // length close to MAX_SNAKE_LEN), this loop could spin for a very long time
    // or even loop forever. Cap at 500 attempts then place food at 0,0 as fallback.
    int attempts = 0;
    while (!valid && attempts < 500) {
        food_x = random(0, GRID_X_MAX + 1);
        food_y = random(0, GRID_Y_MAX + 1);
        valid = true;
        for (int i = 0; i < snake_length; i++) {
            if (food_x == snake_x[i] && food_y == snake_y[i]) {
                valid = false;
                break;
            }
        }
        attempts++;
    }
    // Fallback if grid is completely full (extremely unlikely but safe)
    if (!valid) { food_x = 0; food_y = 0; }
}

// Reset the game variables for a new run
static void resetGame() {
    snake_length = 3;
    snake_x[0] = 15; snake_y[0] = 7;
    snake_x[1] = 14; snake_y[1] = 7;
    snake_x[2] = 13; snake_y[2] = 7;
    current_direction = DIR_RIGHT;
    next_direction = DIR_RIGHT;
    input_locked = false;
    // FIX: Restore speed to the difficulty-selected value, not the scaled-up value
    // from the end of the previous run. Without this, every retry started faster.
    base_speed = diff_base_speed;
    spawnFood();
    redrawNeeded = true;
    needsStatusRedraw = true;
}


void GameSnake_Init() {
    gameState = STATE_WELCOME;
    redrawNeeded = true;
    needsStatusRedraw = true;
}

// Draw the secondary status display
static void drawStatusDisplay() {
    oled2.clearDisplay();
    oled2.setTextColor(SSD1306_WHITE);
    oled2.setTextSize(1);

    if (gameState == STATE_WELCOME || gameState == STATE_DIFFICULTY) {
        oled2.setCursor(0, 10);
        oled2.print("Press SELECT");
        oled2.setCursor(0, 20);
        oled2.print("to Play");
    } else if (gameState == STATE_GAMEOVER) {
        oled2.setCursor(0, 10);
        oled2.print("GAME OVER");
        oled2.setCursor(0, 20);
        oled2.print("Score: ");
        oled2.print((snake_length - 3) * 5);
    } else {
        // Playing
        oled2.setCursor(10, 12);
        oled2.print("Score: ");
        oled2.print((snake_length - 3) * 5);

        oled2.setCursor(75, 12);
        oled2.print("Spd: ");
        oled2.print(base_speed);
    }
    oled2.display();
    needsStatusRedraw = false;
}

void GameSnake_Update() {
    if (needsStatusRedraw) {
        drawStatusDisplay();
    }

    if (gameState == STATE_WELCOME) {
        if (redrawNeeded) {
            oled.clearDisplay();
            oled.setTextColor(SSD1306_WHITE);
            oled.setTextSize(2);
            oled.setCursor(35, 20);
            oled.print("SNAKE");
            oled.setTextSize(1);
            oled.setCursor(25, 45);
            oled.print("Press SELECT");
            oled.display();
            redrawNeeded = false;
        }
        return;
    }
    
    if (gameState == STATE_DIFFICULTY) {
        if (redrawNeeded) {
            oled.clearDisplay();
            oled.setTextColor(SSD1306_WHITE);
            oled.setTextSize(1);
            oled.setCursor(20, 5);
            oled.print("Select Difficulty");
            
            oled.setCursor(45, 25);
            if (diff_sel == 0) oled.print("> Easy <"); else oled.print("  Easy  ");
            
            oled.setCursor(45, 35);
            if (diff_sel == 1) oled.print("> Med  <"); else oled.print("  Med   ");
            
            oled.setCursor(45, 45);
            if (diff_sel == 2) oled.print("> Hard <"); else oled.print("  Hard  ");
            
            oled.display();
            redrawNeeded = false;
        }
        return;
    }

    if (gameState == STATE_GAMEOVER) {
        if (redrawNeeded) {
            oled.clearDisplay();
            oled.setTextColor(SSD1306_WHITE);
            // FIX 25: Show final score on game over screen so the player knows their result.
            oled.setTextSize(2);
            oled.setCursor(10, 10);
            oled.print("GAME OVER");
            oled.setTextSize(1);
            oled.setCursor(20, 38);
            oled.print("Score: ");
            oled.print((snake_length - 3) * 5);
            oled.setCursor(15, 50);
            oled.print("SELECT to Retry");
            oled.display();
            redrawNeeded = false;
        }
        return;
    }

    // gameState == STATE_PLAYING
    unsigned long now = millis();
    if (now - lastMoveTime >= base_speed) {
        lastMoveTime = now;

        // Commit direction change
        current_direction = next_direction;
        input_locked = false;

        // Calculate new head position
        int head_x = snake_x[0];
        int head_y = snake_y[0];

        if (current_direction == DIR_UP) head_y -= 1;
        if (current_direction == DIR_DOWN) head_y += 1;
        if (current_direction == DIR_LEFT) head_x -= 1;
        if (current_direction == DIR_RIGHT) head_x += 1;

        // Wall collision check
        if (head_x < 0 || head_x > GRID_X_MAX || head_y < 0 || head_y > GRID_Y_MAX) {
            gameState = STATE_GAMEOVER;
            redrawNeeded = true;
            needsStatusRedraw = true;
            return;
        }

        // FIX 21: Self collision — check ALL body segments including the very last one.
        // The original code used `snake_length - 1` which skips the tail segment,
        // allowing the head to illegally overlap with it.
        for (int i = 0; i < snake_length; i++) {
            if (head_x == snake_x[i] && head_y == snake_y[i]) {
                gameState = STATE_GAMEOVER;
                redrawNeeded = true;
                needsStatusRedraw = true;
                return;
            }
        }

        // Food check
        if (head_x == food_x && head_y == food_y) {
            if (snake_length < MAX_SNAKE_LEN) {
                snake_length++;
            }
            spawnFood();
            beepTap(); // Beep when eating
            // FIX 22: Speed scaling — game gets progressively faster as the snake grows.
            // Clamps at a minimum of 60ms (very hard) to remain playable.
            // Formula: base_speed decreases by 5ms every 5 food items eaten.
            int foodEaten = snake_length - 3;
            int newSpeed = base_speed - (foodEaten / 5) * 5;
            if (newSpeed < 60) newSpeed = 60;
            base_speed = (unsigned int)newSpeed;
            needsStatusRedraw = true;
        }

        // Move body
        for (int i = snake_length - 1; i > 0; i--) {
            snake_x[i] = snake_x[i - 1];
            snake_y[i] = snake_y[i - 1];
        }

        // Move head
        snake_x[0] = head_x;
        snake_y[0] = head_y;

        redrawNeeded = true;
    }

    // Draw the frame if it changed
    if (redrawNeeded) {
        oled.clearDisplay();
        
        int block_size = 4;
        int cam_x = 0;
        int cam_y = 0;
        
        // Dynamic Zoom Logic
        if (snake_length < 15) {
            block_size = 8;
            // Center camera on head for 8x8 blocks (screen fits 16x8 logical blocks)
            cam_x = snake_x[0] - 8;
            cam_y = snake_y[0] - 4;
            // Clamp camera
            if (cam_x < 0) cam_x = 0;
            if (cam_x > 16) cam_x = 16;
            if (cam_y < 0) cam_y = 0;
            if (cam_y > 8) cam_y = 8;
        } else if (snake_length < 30) {
            block_size = 6;
            // Center camera for 6x6 blocks (screen fits 21x10 logical blocks)
            cam_x = snake_x[0] - 10;
            cam_y = snake_y[0] - 5;
            // Clamp camera
            if (cam_x < 0) cam_x = 0;
            if (cam_x > 11) cam_x = 11; // 32 - 21 = 11
            if (cam_y < 0) cam_y = 0;
            if (cam_y > 6) cam_y = 6;  // 16 - 10 = 6
        }
        
        // Draw play area boundary frame (only visible if within camera view)
        int border_sx = (0 - cam_x) * block_size;
        int border_sy = (0 - cam_y) * block_size;
        int border_w = (GRID_X_MAX + 1) * block_size;
        int border_h = (GRID_Y_MAX + 1) * block_size;
        oled.drawRect(border_sx, border_sy, border_w, border_h, SSD1306_WHITE);

        // Draw food
        int fsx = (food_x - cam_x) * block_size;
        int fsy = (food_y - cam_y) * block_size;
        if (fsx >= 0 && fsx < 128 && fsy >= 0 && fsy < 64) {
            oled.fillRect(fsx, fsy, block_size, block_size, SSD1306_WHITE);
        }

        // Draw snake
        for (int i = 0; i < snake_length; i++) {
            int sx = (snake_x[i] - cam_x) * block_size;
            int sy = (snake_y[i] - cam_y) * block_size;
            
            if (sx >= 0 && sx < 128 && sy >= 0 && sy < 64) {
                if (i == 0) {
                    oled.fillRect(sx, sy, block_size, block_size, SSD1306_WHITE);
                } else {
                    oled.drawRect(sx, sy, block_size, block_size, SSD1306_WHITE);
                }
            }
        }

        oled.display();
        redrawNeeded = false;
    }
}

void GameSnake_HandleInput(LogicalEvent ev) {
    if (gameState == STATE_WELCOME || gameState == STATE_GAMEOVER) {
        if (ev == EV_CENTER_TAP) {
            if (gameState == STATE_WELCOME) {
                gameState = STATE_DIFFICULTY;
            } else {
                gameState = STATE_WELCOME;
            }
            redrawNeeded = true;
        }
        return;
    }
    
    if (gameState == STATE_DIFFICULTY) {
        if (ev == EV_UP_TAP) {
            diff_sel = (diff_sel - 1 + 3) % 3;
            redrawNeeded = true;
        } else if (ev == EV_DOWN_TAP) {
            diff_sel = (diff_sel + 1) % 3;
            redrawNeeded = true;
        } else if (ev == EV_CENTER_TAP) {
            if (diff_sel == 0) { base_speed = 220; diff_base_speed = 220; } // Easy
            else if (diff_sel == 1) { base_speed = 140; diff_base_speed = 140; } // Med
            else { base_speed = 80; diff_base_speed = 80; } // Hard
            
            resetGame();
            gameState = STATE_PLAYING;
            lastMoveTime = millis();
        }

        return;
    }

    // In game routing
    if (input_locked) return;

    if (ev == EV_UP_TAP || ev == EV_UP_HOLD) {
        if (current_direction != DIR_DOWN) { next_direction = DIR_UP; input_locked = true; }
    } else if (ev == EV_DOWN_TAP || ev == EV_DOWN_HOLD) {
        if (current_direction != DIR_UP) { next_direction = DIR_DOWN; input_locked = true; }
    } else if (ev == EV_LEFT_TAP || ev == EV_LEFT_HOLD) {
        if (current_direction != DIR_RIGHT) { next_direction = DIR_LEFT; input_locked = true; }
    } else if (ev == EV_RIGHT_TAP || ev == EV_RIGHT_HOLD) {
        if (current_direction != DIR_LEFT) { next_direction = DIR_RIGHT; input_locked = true; }
    }
}
