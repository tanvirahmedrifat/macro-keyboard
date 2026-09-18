#include "system_input.h"
#include <Arduino.h>

const unsigned long BTN_HOLD_MS = 600;
const unsigned long BTN_DEBOUNCE_MS = 80;

struct BtnState {
    uint8_t pin;
    unsigned long pressTime;
    unsigned long releaseTime;
    bool isPressed;
    bool holdFired;
    bool hold5sFired;
    LogicalEvent tapEv, holdEv, hold5sEv;
    
    LogicalEvent update(unsigned long now) {
        bool rawState = (digitalRead(pin) == LOW);
        if (rawState) {
            if (!isPressed) {
                if (now - releaseTime >= BTN_DEBOUNCE_MS) {
                    isPressed = true;
                    pressTime = now;
                    holdFired = false;
                    hold5sFired = false;
                }
            } else {
                if (!holdFired && (now - pressTime >= BTN_HOLD_MS)) {
                    holdFired = true;
                    return holdEv;
                }
                if (hold5sEv != EV_NONE && !hold5sFired && (now - pressTime >= 5000)) {
                    hold5sFired = true;
                    return hold5sEv;
                }
            }
        } else {
            if (isPressed) {
                isPressed = false;
                releaseTime = now;
                if (!holdFired && !hold5sFired && (now - pressTime >= BTN_DEBOUNCE_MS)) {
                    return tapEv;
                }
            }
        }
        return EV_NONE;
    }
};

static BtnState btns[5] = {
    {PIN1, 0, 0, false, false, false, EV_UP_TAP, EV_UP_HOLD, EV_NONE},
    {PIN2, 0, 0, false, false, false, EV_DOWN_TAP, EV_DOWN_HOLD, EV_NONE},
    {PIN3, 0, 0, false, false, false, EV_LEFT_TAP, EV_LEFT_HOLD, EV_NONE},
    {PIN4, 0, 0, false, false, false, EV_RIGHT_TAP, EV_RIGHT_HOLD, EV_NONE},
    {PIN5, 0, 0, false, false, false, EV_CENTER_TAP, EV_CENTER_HOLD, EV_CENTER_HOLD_5S}
};

void SystemInput_Init() {}

LogicalEvent SystemInput_Update() {
    unsigned long now = millis();
    for (int i=0; i<5; i++) {
        LogicalEvent ev = btns[i].update(now);
        if (ev != EV_NONE) return ev;
    }
    return EV_NONE;
}

void SystemInput_ResetState() {
    for (int i=0; i<5; i++) {
        btns[i].isPressed = false;
        btns[i].holdFired = false;
        btns[i].hold5sFired = false;
    }
}
