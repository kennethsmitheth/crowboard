// Touch event types shared by the HAL touch driver and the UI. No Arduino dependencies.
#pragma once

#include <stdint.h>

namespace checkers::ui {

enum class TouchType : uint8_t { Down, Move, Up, Tap };

struct TouchEvent {
  TouchType type;
  int16_t x;
  int16_t y;
  uint32_t t;  // millis() timestamp
};

}  // namespace checkers::ui
