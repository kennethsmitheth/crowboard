// GT911 touch polling through LovyanGFX, turned into debounced TouchEvents.
#pragma once

#include <stdint.h>

#include "ui/TapDetector.h"
#include "ui/TouchEvent.h"

class LGFX;

namespace checkers::hal {

class Touch {
 public:
  // Probes the GT911 over I2C. Returns false (and logs once) if no controller is configured or
  // it does not answer; poll() is then a no-op and rendering is unaffected.
  bool begin(LGFX& gfx);
  bool present() const { return present_; }
  // Call every loop iteration with millis().
  void poll(uint32_t nowMs);
  bool pop(ui::TouchEvent& out) { return det_.pop(out); }

 private:
  LGFX* gfx_ = nullptr;
  bool present_ = false;
  ui::TapDetector det_;
};

}  // namespace checkers::hal
