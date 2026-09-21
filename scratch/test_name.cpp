#include <Arduino.h>
#include "names_data.h"

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("Testing names:");
    String s = String(firstNames[0]);
    s.toLowerCase();
    Serial.println(s);
}
void loop() {}
