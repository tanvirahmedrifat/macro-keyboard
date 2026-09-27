#include "game_pong.h"
#include "app_games.h"

#define STATE_WELCOME  0
#define STATE_PLAYING  1
#define STATE_GAMEOVER 2

static const int PAD_H = 14;
static const int PAD_W = 3;
static const int WIN_SCORE = 7;

static float bx = 64, by = 32;
static float vx = 2.5f, vy = 1.8f;
static int pY = 25, cY = 25;
static int pScore = 0, cScore = 0;

static int pongState = STATE_WELCOME;
static unsigned long lastFrameTime = 0;
static bool redrawNeeded = true;
static bool needsStatusRedraw = true;

// Player inputs are read directly from pins

static void resetPong() {
    bx = 64; by = 32;
    vx = 2.5f; vy = 1.8f;
    pY = 25; cY = 25;
    pScore = 0; cScore = 0;
    pongState = STATE_WELCOME;
    redrawNeeded = true;
    needsStatusRedraw = true;
}

void GamePong_Init() {
    resetPong();
}

static void drawPongStatus() {
    oled2.clearDisplay();
    oled2.setTextColor(SSD1306_WHITE);
    oled2.setTextSize(1);
    
    if (pongState == STATE_WELCOME) {
        oled2.setCursor(0, 10);
        oled2.print("Press SELECT");
        oled2.setCursor(0, 20);
        oled2.print("to Play");
    } else if (pongState == STATE_GAMEOVER) {
        oled2.setCursor(0, 10);
        oled2.print("GAME OVER");
        oled2.setCursor(0, 20);
        if (pScore >= WIN_SCORE) oled2.print("YOU WIN!");
        else oled2.print("CPU WINS");
    } else {
        // Playing - keep it top-left to avoid the damaged bottom-right area
        oled2.setCursor(0, 0);
        oled2.print("P1:");
        oled2.print(pScore);
        
        oled2.setCursor(75, 0);
        oled2.print("CPU:");
        oled2.print(cScore);
    }
    oled2.display();
    needsStatusRedraw = false;
}

void GamePong_Update() {
    if (needsStatusRedraw) {
        drawPongStatus();
    }

    if (pongState == STATE_WELCOME) {
        if (redrawNeeded) {
            oled.clearDisplay();
            oled.setTextColor(SSD1306_WHITE);
            oled.setTextSize(2);
            oled.setCursor(35, 15);
            oled.print("PONG");
            
            oled.setTextSize(1);
            oled.setCursor(10, 45);
            oled.print("First to 7 wins!");
            oled.display();
            redrawNeeded = false;
        }
        return;
    }
    
    if (pongState == STATE_GAMEOVER) {
        if (redrawNeeded) {
            oled.clearDisplay();
            oled.setTextColor(SSD1306_WHITE);
            oled.setTextSize(2);
            if (pScore >= WIN_SCORE) {
                oled.setCursor(15, 10);
                oled.print("YOU WIN!");
            } else {
                oled.setCursor(15, 10);
                oled.print("CPU WINS");
            }
            // FIX 24: Show restart prompt so player isn't left wondering what to do.
            oled.setTextSize(1);
            oled.setCursor(10, 40);
            oled.print("SELECT: Play Again");
            oled.display();
            redrawNeeded = false;
        }
        return;
    }

    // GAMEPLAY UPDATE (approx 60 fps)
    unsigned long now = millis();
    if (now - lastFrameTime >= 16) {
        lastFrameTime = now;
        
        // Player Movement
        if (digitalRead(PIN1) == LOW) pY = max(0, pY - 3);
        if (digitalRead(PIN2) == LOW) pY = min(SCREEN_H - PAD_H, pY + 3);

        // FIX 23: AI humanization — cap the AI paddle speed so it can be beaten.
        // The old code moved the paddle until `mid` exactly reached the ball Y,
        // making perfect tracking possible at any speed. Capping movement at 2px
        // per frame (same as the ball's initial vy) gives the player a fair chance.
        int mid = cY + PAD_H / 2;
        int aiSpeed = 2; // pixels per frame — deliberately limited to be beatable
        if (mid < (int)by - 2) cY = min(SCREEN_H - PAD_H, cY + aiSpeed);
        if (mid > (int)by + 2) cY = max(0, cY - aiSpeed);

        // Ball Physics
        bx += vx;
        by += vy;

        // Top/Bottom bounce
        if (by <= 0) {
            vy = fabsf(vy);
            by = 0;
            beepTap(); // 500hz
        }
        if (by >= SCREEN_H - 3) {
            vy = -fabsf(vy);
            by = SCREEN_H - 3;
            beepTap(); // 500hz
        }

        // Player Paddle Collision
        if (vx < 0 && bx <= 4 + PAD_W && bx >= 4 && by + 2 >= pY && by <= pY + PAD_H) {
            vx = fabsf(vx) * 1.05f;
            vy += ((by - (pY + PAD_H / 2.0f)) / (PAD_H / 2.0f)) * 1.5f;
            if (vy < -4.5f) vy = -4.5f;
            if (vy > 4.5f) vy = 4.5f;
            if (vx > 5.0f) vx = 5.0f;
            bx = 4 + PAD_W;
            beepTap(); // 1000hz
        }

        // CPU Paddle Collision
        if (bx >= 121 - PAD_W && bx <= 122 && by + 2 >= cY && by <= cY + PAD_H) {
            vx = -fabsf(vx) * 1.05f;
            vy += ((by - (cY + PAD_H / 2.0f)) / (PAD_H / 2.0f)) * 1.2f;
            if (vy < -4.0f) vy = -4.0f;
            if (vy > 4.0f) vy = 4.0f;
            if (vx < -5.0f) vx = -5.0f;
            bx = 121 - PAD_W - 1;
            beepTap(); // 900hz
        }

        // Scoring
        if (bx < 0) {
            cScore++;
            beepTap();
            bx = 64; by = 32;
            // BUG-27 FIX: vy accumulated from multiple paddle hits and was NOT reset on score.
            // After several rallies vy could be ±4.5f permanently, making the ball vertically
            // unplayable. Reset both velocities fresh on each point.
            vx = 2.5f; vy = (random(2) == 0 ? 1.8f : -1.8f);
            pY = 25; cY = 25;
            needsStatusRedraw = true;
        }
        if (bx > SCREEN_W) {
            pScore++;
            beepTap();
            bx = 64; by = 32;
            vx = -2.5f; vy = (random(2) == 0 ? 1.8f : -1.8f); // BUG-27 FIX: reset vy
            pY = 25; cY = 25;
            needsStatusRedraw = true;
        }

        // Win Condition
        if (pScore >= WIN_SCORE || cScore >= WIN_SCORE) {
            pongState = STATE_GAMEOVER;
            needsStatusRedraw = true;
            redrawNeeded = true;
            return;
        }

        redrawNeeded = true;
    }
    
    // Draw Frame
    if (redrawNeeded) {
        oled.clearDisplay();
        
        // Dashed net
        for (int y = 0; y < SCREEN_H; y += 6) {
            oled.drawPixel(63, y, SSD1306_WHITE);
        }
        
        // Paddles
        oled.fillRect(4, pY, PAD_W, PAD_H, SSD1306_WHITE);
        oled.fillRect(121, cY, PAD_W, PAD_H, SSD1306_WHITE);
        
        // Ball
        oled.fillRect((int)bx, (int)by, 3, 3, SSD1306_WHITE);
        
        oled.display();
        redrawNeeded = false;
    }
}

void GamePong_HandleInput(LogicalEvent ev) {
    if (pongState == STATE_WELCOME || pongState == STATE_GAMEOVER) {
        if (ev == EV_CENTER_TAP) {
            bx = 64; by = 32;
            vx = 2.5f; vy = 1.8f;
            pY = 25; cY = 25;
            pScore = 0; cScore = 0;
            pongState = STATE_PLAYING;
            lastFrameTime = millis();
            needsStatusRedraw = true;
        }
        return;
    }
    
    // Hardware pin reads are handled directly in GamePong_Update.
}
