#include "game_tetris.h"
#include "app_games.h"
#include <pgmspace.h>

#define STATE_WELCOME  0
#define STATE_PLAYING  1
#define STATE_GAMEOVER 2

#define TT_COLS 10
#define TT_ROWS 16
#define TT_SZ 4
#define TT_OX 44
#define TT_OY 0

struct TetPiece {
    int8_t x, y;
    uint8_t type;
    int8_t cells[4][2];
    
    void load(uint8_t t);
    void getAbs(int8_t out[4][2]) const {
        for (int i = 0; i < 4; i++) {
            out[i][0] = x + cells[i][0];
            out[i][1] = y + cells[i][1];
        }
    }
};

static const int8_t PIECES[7][4][2] PROGMEM = {
    {{0, 0}, {1, 0}, {2, 0}, {3, 0}}, // I
    {{0, 0}, {0, 1}, {1, 1}, {2, 1}}, // J
    {{2, 0}, {0, 1}, {1, 1}, {2, 1}}, // L
    {{0, 0}, {1, 0}, {1, 1}, {2, 1}}, // S
    {{1, 0}, {2, 0}, {0, 1}, {1, 1}}, // Z
    {{1, 0}, {0, 1}, {1, 1}, {2, 1}}, // T
    {{0, 0}, {1, 0}, {0, 1}, {1, 1}}, // O
};

static uint8_t board[TT_ROWS][TT_COLS];

void TetPiece::load(uint8_t t) {
    type = t;
    for (int i = 0; i < 4; i++) {
        cells[i][0] = pgm_read_byte(&PIECES[t][i][0]);
        cells[i][1] = pgm_read_byte(&PIECES[t][i][1]);
    }
}

static bool ttFits(const TetPiece &p, int dx, int dy) {
    for (int i = 0; i < 4; i++) {
        int nx = p.x + p.cells[i][0] + dx;
        int ny = p.y + p.cells[i][1] + dy;
        if (nx < 0 || nx >= TT_COLS || ny >= TT_ROWS)
            return false;
        if (ny >= 0 && board[ny][nx])
            return false;
    }
    return true;
}

static void ttRotate(TetPiece &p) {
    int8_t tmp[4][2];
    for (int i = 0; i < 4; i++) {
        int8_t rx = p.cells[i][0] - p.cells[0][0];
        int8_t ry = p.cells[i][1] - p.cells[0][1];
        tmp[i][0] = p.cells[0][0] - ry;
        tmp[i][1] = p.cells[0][1] + rx;
    }
    int8_t saved[4][2];
    memcpy(saved, p.cells, sizeof(saved));
    memcpy(p.cells, tmp, sizeof(tmp));
    if (!ttFits(p, 0, 0))
        memcpy(p.cells, saved, sizeof(saved));
}

static int tetrisState = STATE_WELCOME;
static uint16_t score = 0;
static uint8_t level = 1;
static uint32_t dropInterval = 600, lastDrop = 0, lastMove = 0;
static TetPiece cur, next;

static bool redrawNeeded = true;
static bool needsStatusRedraw = true;

static void resetTetris() {
    memset(board, 0, sizeof(board));
    score = 0;
    level = 1;
    dropInterval = 600;
    lastDrop = millis();
    lastMove = millis();
    
    cur.load(random(0, 7));
    cur.x = TT_COLS / 2 - 1;
    cur.y = 0;
    next.load(random(0, 7));
    next.x = TT_COLS / 2 - 1;
    next.y = 0;
    
    tetrisState = STATE_WELCOME;
    redrawNeeded = true;
    needsStatusRedraw = true;
}

void GameTetris_Init() {
    resetTetris();
}

static void drawTetrisStatus() {
    oled2.clearDisplay();
    oled2.setTextColor(SSD1306_WHITE);
    oled2.setTextSize(1);
    
    if (tetrisState == STATE_WELCOME) {
        oled2.setCursor(0, 10);
        oled2.print("Press SELECT");
        oled2.setCursor(0, 20);
        oled2.print("to Play");
    } else if (tetrisState == STATE_GAMEOVER) {
        oled2.setCursor(0, 10);
        oled2.print("GAME OVER");
        oled2.setCursor(0, 20);
        oled2.print("Score: ");
        oled2.print(score);
    } else {
        // Playing
        // Display score on the safe top line
        oled2.setCursor(0, 0);
        oled2.print("SCR:");
        oled2.print(score);
        
        oled2.setCursor(75, 0);
        oled2.print("LVL:");
        oled2.print(level);
        
        // Draw the NEXT piece safely on the left side
        oled2.setCursor(0, 15);
        oled2.print("NXT:");
        
        int nxtX = 30;
        int nxtY = 15;
        for (int i = 0; i < 4; i++) {
            int cx = next.cells[i][0] * TT_SZ;
            int cy = next.cells[i][1] * TT_SZ;
            oled2.fillRect(nxtX + cx, nxtY + cy, TT_SZ - 1, TT_SZ - 1, SSD1306_WHITE);
        }
    }
    oled2.display();
    needsStatusRedraw = false;
}

void GameTetris_Update() {
    if (needsStatusRedraw) {
        drawTetrisStatus();
    }
    
    if (tetrisState == STATE_WELCOME) {
        if (redrawNeeded) {
            oled.clearDisplay();
            oled.setTextColor(SSD1306_WHITE);
            oled.setTextSize(2);
            oled.setCursor(25, 15);
            oled.print("TETRIS");
            
            oled.setTextSize(1);
            oled.setCursor(15, 45);
            oled.print("Hold * to exit");
            oled.display();
            redrawNeeded = false;
        }
        return;
    }
    
    if (tetrisState == STATE_GAMEOVER) {
        if (redrawNeeded) {
            oled.clearDisplay();
            oled.setTextColor(SSD1306_WHITE);
            oled.setTextSize(2);
            oled.setCursor(10, 25);
            oled.print("GAME OVER!");
            oled.display();
            redrawNeeded = false;
        }
        return;
    }
    
    // Playing State
    uint32_t now = millis();
    
    // Continuous movement for actual pins:
    
    // Let's correct continuous movement for actual pins:
    if (now - lastMove > 130) {
        if (digitalRead(PIN3) == LOW && ttFits(cur, -1, 0)) { // LEFT
            cur.x--;
            lastMove = now;
            redrawNeeded = true;
        }
        if (digitalRead(PIN4) == LOW && ttFits(cur, 1, 0)) { // RIGHT
            cur.x++;
            lastMove = now;
            redrawNeeded = true;
        }
    }
    
    uint32_t interval = (digitalRead(PIN2) == LOW) ? 60 : dropInterval; // DOWN is fast drop
    
    if (now - lastDrop > interval) {
        lastDrop = now;
        if (ttFits(cur, 0, 1)) {
            cur.y++;
        } else {
            int8_t abs[4][2];
            cur.getAbs(abs);
            for (int i = 0; i < 4; i++) {
                if (abs[i][1] >= 0) {
                    board[abs[i][1]][abs[i][0]] = 1;
                }
            }
            beepTap(); // Beep when piece locks
            
            int cleared = 0;
            for (int r = TT_ROWS - 1; r >= 0; r--) {
                bool full = true;
                for (int c = 0; c < TT_COLS; c++) {
                    if (!board[r][c]) {
                        full = false;
                        break;
                    }
                }
                if (full) {
                    cleared++;
                    for (int rr = r; rr > 0; rr--) {
                        memcpy(board[rr], board[rr - 1], TT_COLS);
                    }
                    memset(board[0], 0, TT_COLS);
                    r++;
                }
            }
            
            if (cleared) {
                static const uint16_t pts[5] = {0, 40, 100, 300, 1200};
                score += pts[min(cleared, 4)] * level;
                level = 1 + score / 200;
                dropInterval = max(80U, 600U - (level - 1) * 60);
                beepTap(); // Extra beep for clear
                needsStatusRedraw = true;
            }
            
            cur = next;
            cur.x = TT_COLS / 2 - 1;
            cur.y = 0;
            next.load(random(0, 7));
            needsStatusRedraw = true;
            
            if (!ttFits(cur, 0, 0)) {
                tetrisState = STATE_GAMEOVER;
                needsStatusRedraw = true;
            }
        }
        redrawNeeded = true;
    }
    
    if (redrawNeeded) {
        oled.clearDisplay();
        
        // Draw frame
        
        
        // Draw board
        for (int r = 0; r < TT_ROWS; r++) {
            for (int c = 0; c < TT_COLS; c++) {
                if (board[r][c]) {
                    oled.fillRect(TT_OX + c * TT_SZ, TT_OY + r * TT_SZ, TT_SZ - 1, TT_SZ - 1, SSD1306_WHITE);
                }
            }
        }
        
        // Draw current piece
        int8_t abs[4][2];
        cur.getAbs(abs);
        for (int i = 0; i < 4; i++) {
            if (abs[i][1] >= 0) {
                oled.fillRect(TT_OX + abs[i][0] * TT_SZ, TT_OY + abs[i][1] * TT_SZ, TT_SZ - 1, TT_SZ - 1, SSD1306_WHITE);
            }
        }
        
        // Draw ghost piece
        TetPiece ghost = cur;
        while (ttFits(ghost, 0, 1)) {
            ghost.y++;
        }
        ghost.getAbs(abs);
        for (int i = 0; i < 4; i++) {
            if (abs[i][1] >= 0 && abs[i][1] != cur.y + cur.cells[i][1]) {
                oled.drawRect(TT_OX + abs[i][0] * TT_SZ + 1, TT_OY + abs[i][1] * TT_SZ + 1, TT_SZ - 2, TT_SZ - 2, SSD1306_WHITE);
            }
        }
        
        // --- DRAW FULL SCREEN UI ---
        // Left Panel (Score/Level)
        oled.drawFastVLine(42, 0, 64, SSD1306_WHITE);
        oled.drawFastVLine(85, 0, 64, SSD1306_WHITE);
        
        oled.setTextSize(1);
        oled.setCursor(2, 5);
        oled.print("SCR:");
        oled.setCursor(2, 15);
        oled.print(score);
        
        oled.setCursor(2, 35);
        oled.print("LVL:");
        oled.setCursor(2, 45);
        oled.print(level);
        
        // Right Panel (Next Piece)
        oled.setCursor(95, 5);
        oled.print("NXT");
        
        int nxtX = 98;
        int nxtY = 25;
        for (int i = 0; i < 4; i++) {
            int cx = next.cells[i][0] * 5; // Slightly larger for next piece
            int cy = next.cells[i][1] * 5;
            oled.fillRect(nxtX + cx, nxtY + cy, 4, 4, SSD1306_WHITE);
        }
        
        oled.display();
        redrawNeeded = false;
    }
}

void GameTetris_HandleInput(LogicalEvent ev) {
    if (tetrisState == STATE_WELCOME || tetrisState == STATE_GAMEOVER) {
        if (ev == EV_CENTER_TAP) {
            resetTetris();
            tetrisState = STATE_PLAYING;
            lastMove = millis();
            lastDrop = millis();
        }
        return;
    }
    
    // In game routing
    if (ev == EV_UP_TAP) {
        ttRotate(cur);
        beepTap();
        redrawNeeded = true;
    }
}
