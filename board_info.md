# NodeMCU ESP Board - Macro Keyboard Info

## Can I make a macro keyboard with a NodeMCU ESP board?
Yes, it is possible, but **it depends on the exact model of your ESP board** and how you want to connect it to your iPhone.

### 1. Using Bluetooth (Recommended for iPhone)
If your board is an **ESP32** (which has built-in Bluetooth), you can easily use it to create a wireless Bluetooth (BLE) macro keyboard that pairs directly with your iPhone.
*   **Library needed:** `ESP32-BLE-Keyboard` (Available in Arduino IDE).
*   **Pros:** Connects natively to iOS as a standard Bluetooth keyboard. No cables needed.
*   **Cons:** Requires an ESP32 (e.g., ESP-WROOM-32), not the older ESP8266.

### 2. Using USB Cable (Wired)
If you want to plug the board directly into your iPhone using a USB to Lightning/USB-C adapter:
*   **ESP32-S2 or ESP32-S3:** These newer ESP boards have native USB hardware (USB OTG) and can act as a standard USB HID Keyboard.
*   **Standard NodeMCU ESP8266 or older ESP32:** These do **not** have native USB HID support. The USB port on them is only connected to a Serial-to-USB converter chip (like CH340 or CP2102) for programming. They cannot be recognized as a USB keyboard by an iPhone via cable.

### Conclusion
To make a macro keyboard for an iPhone, the best and most reliable method is to use an **ESP32 over Bluetooth**.

---
*Note: I attempted to automatically detect your connected board over USB, but there was a system error restricting me from reading the USB ports. Could you check the large square chip on your board and let me know if it says "ESP8266" or "ESP32"?*
