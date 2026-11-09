// GT911 touch polling through LovyanGFX, turned into debounced TouchEvents.
#pragma once

#include <stdint.h>

#include "ui/TapDetector.h"
#include "ui/TouchEvent.h"

class LGFX;

namespace checkers::hal {

class Touch {
 public:
  // Returns false (and logs once) if the display has no touch controller attached.
  bool begin(LGFX& gfx);
  // Call every loop iteration with millis().
  void poll(uint32_t nowMs);
  bool pop(ui::TouchEvent& out) { return det_.pop(out); }

 private:
  LGFX* gfx_ = nullptr;
  ui::TapDetector det_;
};

}  // namespace checkers::hal
