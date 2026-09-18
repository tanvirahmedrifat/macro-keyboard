import re

with open("game_tetris.cpp", "r") as f:
    content = f.read()

content = re.sub(r'#define TT_COLS \d+', '#define TT_COLS 10', content)
content = re.sub(r'#define TT_ROWS \d+', '#define TT_ROWS 16', content)
content = re.sub(r'#define TT_SZ \d+', '#define TT_SZ 4', content)
content = re.sub(r'#define TT_OX \d+', '#define TT_OX 44', content)
content = re.sub(r'#define TT_OY \d+', '#define TT_OY 0', content)

# Remove the frame drawing from OLED (because we will draw our own)
content = content.replace("oled.drawRect(TT_OX - 1, TT_OY - 1, TT_COLS * TT_SZ + 2, TT_ROWS * TT_SZ + 2, SSD1306_WHITE);", "")

# We need to insert our new UI drawing right before oled.display() in the redrawNeeded block
# Find the exact place.
old_draw = """
        // Draw current piece
        int8_t abs[4][2];
        cur.getAbs(abs);
        for (int i = 0; i < 4; i++) {
            if (abs[i][1] >= 0) {
                oled.fillRect(TT_OX + abs[i][0] * TT_SZ, TT_OY + abs[i][1] * TT_SZ, TT_SZ - 1, TT_SZ - 1, SSD1306_WHITE);
            }
        }
        
        oled.display();
"""
new_draw = """
        // Draw current piece
        int8_t abs[4][2];
        cur.getAbs(abs);
        for (int i = 0; i < 4; i++) {
            if (abs[i][1] >= 0) {
                oled.fillRect(TT_OX + abs[i][0] * TT_SZ, TT_OY + abs[i][1] * TT_SZ, TT_SZ - 1, TT_SZ - 1, SSD1306_WHITE);
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
"""
content = content.replace(old_draw, new_draw)

# We can also simplify drawTetrisStatus() to just keep minimal info on OLED2, or leave it. 
# Leaving it is fine as a secondary screen.

with open("game_tetris.cpp", "w") as f:
    f.write(content)
