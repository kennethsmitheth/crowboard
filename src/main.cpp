// Hello world for the Elecrow CrowPanel ESP32 7.0".
// Fills the screen, draws a greeting, and echoes touches both on screen and over serial.
#include <Arduino.h>
#include "hal/Log.h"
#include "hal/Diag.h"
#include "crowpanel7_lgfx.h"

static LGFX gfx;
static Diag diag;

void setup() {
  Serial.begin(115200);
  delay(300);
  LOG_I("CrowPanel 7.0\" hello world");
  LOG_I("PSRAM: %u bytes total, %u free", ESP.getPsramSize(), ESP.getFreePsram());
  LOG_I("Heap:  %u bytes free", ESP.getFreeHeap());

  if (!gfx.init()) {
    LOG_E("gfx.init() failed");
  }
  gfx.setBrightness(255);
  gfx.fillScreen(TFT_NAVY);

  gfx.setTextColor(TFT_WHITE, TFT_NAVY);
  gfx.setTextDatum(textdatum_t::middle_center);
  gfx.setFont(&fonts::FreeSansBold24pt7b);
  gfx.drawString("Hello, CrowPanel!", gfx.width() / 2, gfx.height() / 2 - 30);

  gfx.setFont(&fonts::FreeSans12pt7b);
  gfx.drawString("800 x 480  -  touch the screen", gfx.width() / 2, gfx.height() / 2 + 30);

  LOG_I("Display ready: %d x %d", gfx.width(), gfx.height());
}

void loop() {
  diag.tick();
  int32_t x, y;
  if (gfx.getTouch(&x, &y)) {
    gfx.fillCircle(x, y, 8, TFT_ORANGE);
    LOG_I("touch %ld,%ld", (long)x, (long)y);
  }
  delay(10);
}
