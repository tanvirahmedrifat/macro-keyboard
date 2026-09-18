# ESP32 Macro Keyboard Wiring Diagram

Based on the `Macro-Keyboard.ino` project, this document contains the wiring for the **5 buttons**, **Two I2C OLED Displays**, and a **Passive Buzzer**.

## Wiring Table

| Component | Component Pin | ESP32 Pin | Note |
| :--- | :--- | :--- | :--- |
| **Button 1** | Leg 1 | **D12** | Tap: First name, Hold: Username |
| | Leg 2 | **GND** | (Code uses `INPUT_PULLUP`) |
| **Button 2** | Leg 1 | **D13** | Tap: Form Macro, Hold: New Profile |
| | Leg 2 | **GND** | |
| **Button 3** | Leg 1 | **D14** | Tap: Password, Hold: Save to Notes |
| | Leg 2 | **GND** | |
| **Button 4** | Leg 1 | **D26** | Spotlight / System Actions |
| | Leg 2 | **GND** | |
| **Button 5** | Leg 1 | **D27** | Tap: Left Arrow, Hold: Right Arrow |
| | Leg 2 | **GND** | |
| **Buzzer** | Positive (+) | **D17** | Optional: add 100Ω-220Ω resistor |
| | Negative (-) | **GND** | |
| **OLED 1 (Main, Blue/Yellow)**| VCC | **3.3V** | Powers the display |
| | GND | **GND** | |
| | SDA | **D21** | ESP32 I2C 0 Data |
| | SCL | **D22** | ESP32 I2C 0 Clock |
| **OLED 2 (Status, Black/White)**| VCC | **3.3V** | Powers the second display |
| | GND | **GND** | |
| | SDA | **D32** | ESP32 I2C 1 Data |
| | SCL | **D33** | ESP32 I2C 1 Clock |

## Visual Wiring Diagram

```text
                               +-------------------+
                               |       ESP32       |
                               |                   |
    [ Button 1 ] ----(Leg 1)-->| D12               |
                 ----(Leg 2)-->| GND               |
                               |                   |
    [ Button 2 ] ----(Leg 1)-->| D13               |
                 ----(Leg 2)-->| GND               |
                               |                   |
    [ Button 3 ] ----(Leg 1)-->| D14               |
                 ----(Leg 2)-->| GND               |
                               |                   |
    [ Button 4 ] ----(Leg 1)-->| D26               |
                 ----(Leg 2)-->| GND               |
                               |                   |
    [ Button 5 ] ----(Leg 1)-->| D27               |
                 ----(Leg 2)-->| GND               |
                               |                   |
    [ Buzzer ]   ----(+)------>| D17               |
                 ----(-)------>| GND               |
                               |                   |
    [ OLED 1 ]   ----(SDA)---->| D21               |
  (Addr: 0x3C)   ----(SCL)---->| D22               |
                 ----(VCC)---->| 3.3V              |
                 ----(GND)---->| GND               |
                               |                   |
    [ OLED 2 ]   ----(SDA)---->| D32               |
  (Addr: 0x3C)   ----(SCL)---->| D33               |
                 ----(VCC)---->| 3.3V              |
                 ----(GND)---->| GND               |
                               +-------------------+
```

## Important Notes:
1. **Buttons:** Because you are using `pinMode(..., INPUT_PULLUP)` in your code, you **do not** need external resistors for the buttons. Simply connect one side of the button to the digital pin, and the other side directly to any GND pin.
2. **Dual I2C Buses:** The ESP32 has two independent I2C buses. We are using both to avoid having to solder address jumpers on the OLEDs! Both OLEDs can stay at their default `0x3C` address.
   - **OLED 1** connects to the default pins: **D21** (SDA) and **D22** (SCL).
   - **OLED 2** connects to the secondary pins we defined in code: **D32** (SDA) and **D33** (SCL).
3. **OLED Power:** Connect VCC of both OLEDs to 3.3V. If your specific OLED module only works with 5V, you can use the VIN/5V pin, but 3.3V is standard and safer.
