#include "hal/Touch.h"

#include "hal/Log.h"
#include "hal/crowpanel7_lgfx.h"

namespace checkers::hal {

bool Touch::begin(LGFX& gfx) {
  gfx_ = &gfx;
  if (gfx.touch() == nullptr) {
    LOG_W("touch: no controller configured; taps disabled");
    gfx_ = nullptr;
    return false;
  }
  // init() is idempotent: it returns true at once if gfx.init() already found the controller,
  // otherwise it retries the I2C probe (both GT911 addresses, 0x14 and 0x5D).
  if (!gfx.touch()->init()) {
    LOG_W("touch: absent (GT911 did not answer on I2C); taps disabled");
    gfx_ = nullptr;
    return false;
  }
  present_ = true;
  LOG_I("touch: ready");
  return true;
}

void Touch::poll(uint32_t nowMs) {
  if (!gfx_) return;
  int32_t x = 0, y = 0;
  bool pressed = gfx_->getTouch(&x, &y);
  det_.feed(pressed, (int16_t)x, (int16_t)y, nowMs);
}

}  // namespace checkers::hal
