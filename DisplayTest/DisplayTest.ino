#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET    -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C // I2C address for the OLED (usually 0x3C)

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void setup() {
  Serial.begin(115200);
  Serial.println("OLED Test Starting...");

  // Initialize the OLED display
  // ESP32 default I2C pins are SDA=D21, SCL=D22
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed. Please check your wiring!"));
    for(;;); // Halt the code here if display isn't found
  }

  // Clear the display buffer
  display.clearDisplay();

  // Draw Title
  display.setTextSize(2);             
  display.setTextColor(SSD1306_WHITE);        
  display.setCursor(0,0);             
  display.println(F("OLED Test"));
  
  // Draw Message
  display.setTextSize(1);
  display.setCursor(0, 25);
  display.println(F("Wiring is Correct!"));
  
  // Draw Pin Info
  display.setCursor(0, 45);
  display.println(F("SDA -> D21"));
  display.println(F("SCL -> D22"));

  // Display everything on the screen
  display.display();
}

void loop() {
  // Draw a blinking square in the bottom right corner to show the loop is running
  display.fillRect(115, 50, 10, 10, SSD1306_INVERSE);
  display.display();
  delay(500);
}
