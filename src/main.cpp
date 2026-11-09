// Hello world for the Elecrow CrowPanel ESP32 7.0".
// Fills the screen, draws a greeting, and echoes touches both on screen and over serial.
#include <Arduino.h>
#include "hal/Log.h"
#include "hal/Diag.h"
#include "hal/SerialCmd.h"
#include "hal/crowpanel7_lgfx.h"
#include "ui/MockScreen.h"
#include "hal/Touch.h"
#include "ui/BoardGeometry.h"

static LGFX gfx;
static Diag diag;
static checkers::hal::Touch touch;
static SerialCmd cmd(gfx, diag);

void setup() {
  Serial.begin(921600);
  delay(300);
  LOG_I("CrowPanel 7.0\" hello world");
  LOG_I("PSRAM: %u bytes total, %u free", ESP.getPsramSize(), ESP.getFreePsram());
  LOG_I("Heap:  %u bytes free", ESP.getFreeHeap());

  if (!gfx.init()) {
    LOG_E("gfx.init() failed");
  }
  gfx.setBrightness(255);
  touch.begin(gfx);
  checkers::ui::drawStaticMock(gfx);

  LOG_I("Display ready: %d x %d", gfx.width(), gfx.height());
}

void loop() {
  diag.tick();
  cmd.tick();
  touch.poll(millis());
  static const char* const kNames[] = {"down", "move", "up", "tap"};
  checkers::ui::TouchEvent ev;
  while (touch.pop(ev)) {
    LOG_I("touch %s %d,%d sq=%d", kNames[(int)ev.type], ev.x, ev.y,
          checkers::ui::cellAt(ev.x, ev.y));
    if (ev.type == checkers::ui::TouchType::Tap) gfx.fillCircle(ev.x, ev.y, 8, TFT_ORANGE);
  }
  delay(10);
}
