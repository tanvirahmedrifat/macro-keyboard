import re

with open('Macro-Keyboard.ino', 'r') as f:
    content = f.read()

# Make sure we use Adafruit_SH1106G for the object
content = re.sub(r'Adafruit_SSD1306 oled\(SCREEN_W, SCREEN_H, &Wire, OLED_RST\);', 'Adafruit_SH1106G oled(128, 64, &Wire, OLED_RST);', content)

# Fix setup initialization to use SH110X begin
content = content.replace('oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)', 'oled.begin(OLED_ADDR, true)')

# Fix colors just in case they were reverted
content = content.replace('SSD1306_WHITE', 'SH110X_WHITE')
content = content.replace('SSD1306_BLACK', 'SH110X_BLACK')

# Update displaySH1106 function to be perfect
new_display_func = """
// ── CUSTOM SH1106 DISPLAY DRIVER ─────────────────────────
int SH1106_OFFSET = 2; // Offset 2 fixes the left-cutoff and right-static!

void displaySH1106() {
  uint8_t *buffer = oled.getBuffer();
  if (!buffer) return;
  
  for (uint8_t page = 0; page < 8; page++) {
    for (uint8_t i = 0; i < 128; i += 32) {
      // Re-assert column address before every chunk to prevent SH1106 skipped columns (vertical stripes)
      Wire.beginTransmission(OLED_ADDR);
      Wire.write(0x00); // Command mode
      Wire.write(0xB0 + page); // Page address
      Wire.write((SH1106_OFFSET + i) & 0x0F); // Lower column
      Wire.write(0x10 | ((SH1106_OFFSET + i) >> 4)); // Upper column
      Wire.endTransmission();
      
      // Send 32 bytes of data
      Wire.beginTransmission(OLED_ADDR);
      Wire.write(0x40); // Data mode
      for (uint8_t j = 0; j < 32; j++) {
        Wire.write(buffer[page * 128 + i + j]);
      }
      Wire.endTransmission();
    }
  }
}
"""
content = re.sub(r'// ── CUSTOM SH1106 DISPLAY DRIVER ─────────────────────────.*?void displaySH1106\(\) \{.*?\}\n\}\n', new_display_func, content, flags=re.DOTALL)

with open('Macro-Keyboard.ino', 'w') as f:
    f.write(content)

print("Done patching Macro-Keyboard.ino")
