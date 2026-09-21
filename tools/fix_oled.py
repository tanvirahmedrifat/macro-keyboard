import re

with open('Macro-Keyboard.ino', 'r') as f:
    content = f.read()

# Replace library
content = content.replace('#include <Adafruit_SH110X.h>', '')
content = content.replace('Adafruit_SH1106G oled(128, 64, &Wire, OLED_RST);', 'Adafruit_SSD1306 oled(SCREEN_W, SCREEN_H, &Wire, OLED_RST);')

# Replace color constants
content = content.replace('SH110X_WHITE', 'SSD1306_WHITE')
content = content.replace('SH110X_BLACK', 'SSD1306_BLACK')

# Replace oled.display() with displaySH1106() EXCEPT in the custom function definition
content = content.replace('oled.display();', 'displaySH1106();')

# Insert the custom function right before drawSlide0
custom_func = """
// ── CUSTOM SH1106 DISPLAY DRIVER ─────────────────────────
int SH1106_OFFSET = 0; // Set to 2 if screen is shifted by 2 pixels!

void displaySH1106() {
  uint8_t *buffer = oled.getBuffer();
  if (!buffer) return;
  for (uint8_t page = 0; page < 8; page++) {
    Wire.beginTransmission(OLED_ADDR);
    Wire.write(0x00); // Command mode
    Wire.write(0xB0 + page); // Page address (0-7)
    Wire.write(SH1106_OFFSET & 0x0F); // Lower column address
    Wire.write(0x10 | (SH1106_OFFSET >> 4)); // Upper column address
    Wire.endTransmission();
    
    for (uint8_t i = 0; i < 128; i += 32) {
      Wire.beginTransmission(OLED_ADDR);
      Wire.write(0x40); // Data mode
      for (uint8_t j = 0; j < 32; j++) {
        Wire.write(buffer[page * 128 + i + j]);
      }
      Wire.endTransmission();
    }
  }
}

// ── SLIDE 0: BIG NAME ─────────────────────────────────────
"""
content = content.replace('// ── SLIDE 0: BIG NAME ─────────────────────────────────────\n', custom_func)

# Fix setup() initialization for oled (it's now SSD1306 again)
content = content.replace('if (!oled.begin(OLED_ADDR, true)) { // true = reset', 'if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {')

with open('Macro-Keyboard.ino', 'w') as f:
    f.write(content)

print("Done patching Macro-Keyboard.ino")
