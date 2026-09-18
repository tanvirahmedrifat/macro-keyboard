#ifndef SYSTEM_INPUT_H
#define SYSTEM_INPUT_H

#include "globals.h"

enum LogicalEvent {
  EV_NONE,
  EV_UP_TAP, EV_UP_HOLD,
  EV_DOWN_TAP, EV_DOWN_HOLD,
  EV_LEFT_TAP, EV_LEFT_HOLD,
  EV_RIGHT_TAP, EV_RIGHT_HOLD,
  EV_CENTER_TAP, EV_CENTER_HOLD,
  EV_CENTER_HOLD_5S,
  EV_BACKSPACE_HOLD_2S
};

void SystemInput_Init();
LogicalEvent SystemInput_Update();

void SystemInput_ResetState();
#endif
