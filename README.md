# Tanvir's ESP32 Macro Keyboard v3.0

A professional BLE macro keyboard built with an **ESP32** board, **3 push buttons**, and a **128x64 I2C OLED display**. Connects wirelessly to iPhone and other devices via Bluetooth. Pastes names, usernames, and strong passwords at human-like typing speed to avoid bot detection.

---

## 📁 Project Files

| File | Description |
|------|-------------|
| `Macro-Keyboard.ino` | Main firmware — all features |
| `names_data.h` | Auto-generated array of 1000 fake names |
| `DisplayTest/DisplayTest.ino` | Standalone OLED test sketch |
| `board_info.md` | ESP32 board compatibility notes |

---

## 🔌 Hardware

| Component | Details |
|-----------|---------|
| Microcontroller | ESP32 (ESP32-D0WD-V3 rev 3.1) |
| Display | 128x64 I2C OLED (SSD1306, address 0x3C) |
| Buttons | 3x push buttons (momentary, active LOW) |
| Connection | Bluetooth BLE (device name: **Tanvir's Keyboard**) |

### Wiring

| Component | ESP32 Pin |
|-----------|----------|
| OLED SDA | D21 |
| OLED SCL | D22 |
| OLED VCC | 3.3V |
| OLED GND | GND |
| Button 1 | D12 → GND |
| Button 2 | D13 → GND |
| Button 3 | D14 → GND |

---

## 🎮 Button Controls

| Button | Press Type | Action |
|--------|-----------|--------|
| **BTN 1** | Short tap | Types `first name` + Enter (lowercase) |
| **BTN 1** | Hold 2 sec | Types `firstnamelastname294` + Enter (username with random digits) |
| **BTN 2** | Short tap | Types `last name` + Enter (lowercase) |
| **BTN 2** | Hold 2 sec | 🔄 Loads a brand new profile (new name + new password) |
| **BTN 3** | Short tap | ⌨️ Types the current password (human-speed) |

---

## 📺 OLED Display Layout

```
+ BLE          #42/1000
────────────────────────
angel vasquez
────────────────────────
pw:Xk9m-_!3aL@2n
────────────────────────
1:fn  H1:user  2:ln
H2:new profile  3:pw
```

---

## 🔐 Password Format

- **Length:** 16 characters
- **Characters:** `A-Z` + `a-z` + `0-9` + `! @ # $ - _`
- **Guarantee:** Always contains at least 1 uppercase, 1 lowercase, 1 digit, 1 special char
- **Shuffle:** Fisher-Yates shuffle for true randomness
- **Compatible with:** Google, Facebook, Instagram, Twitter, Amazon, Apple ID and all major platforms

---

## 🎬 Boot Animation

On power-up, the display shows a typewriter animation of **"Rifat"**, blinks 3 times, then transitions to the main dashboard.

---

## 🛡️ Anti-Bot Detection

All text is typed character-by-character with randomised delays:
- **Base delay:** 38–110ms per keystroke (~40–90 WPM)
- **10% chance:** 100–280ms hesitation pause
- **5% chance:** 12–30ms burst (fast familiar word)
- **Pre-word pause:** 70–200ms
- **Pre-Enter pause:** 90–300ms

Since it's a real physical Bluetooth HID keyboard, it is indistinguishable from a human typist.

---

## 📦 Arduino Libraries Required

Install these from **Arduino IDE → Sketch → Include Library → Manage Libraries:**

| Library | Install Name |
|---------|-------------|
| HijelHID BLE Keyboard | `HijelHID_BLEKeyboard` |
| NimBLE Arduino | `NimBLE-Arduino` |
| Adafruit GFX | `Adafruit GFX Library` |
| Adafruit SSD1306 | `Adafruit SSD1306` |

Also install the **ESP32 board core** via Board Manager: search `esp32` by Espressif Systems.

---

## 🚀 Upload Command (CLI)

```bash
arduino-cli compile --fqbn esp32:esp32:esp32 --port /dev/ttyUSB0 --upload /path/to/Macro-Keyboard/
```

---

*Built by Tanvir | Powered by ESP32*
