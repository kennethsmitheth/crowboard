// Hello world for the Elecrow CrowPanel ESP32 7.0".
// Fills the screen, draws a greeting, and echoes touches both on screen and over serial.
#include <Arduino.h>
#include "crowpanel7_lgfx.h"

static LGFX gfx;

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("CrowPanel 7.0\" hello world");
  Serial.printf("PSRAM: %u bytes total, %u free\n", ESP.getPsramSize(), ESP.getFreePsram());
  Serial.printf("Heap:  %u bytes free\n", ESP.getFreeHeap());

  if (!gfx.init()) {
    Serial.println("gfx.init() failed");
  }
  gfx.setBrightness(255);
  gfx.fillScreen(TFT_NAVY);

  gfx.setTextColor(TFT_WHITE, TFT_NAVY);
  gfx.setTextDatum(textdatum_t::middle_center);
  gfx.setFont(&fonts::FreeSansBold24pt7b);
  gfx.drawString("Hello, CrowPanel!", gfx.width() / 2, gfx.height() / 2 - 30);

  gfx.setFont(&fonts::FreeSans12pt7b);
  gfx.drawString("800 x 480  -  touch the screen", gfx.width() / 2, gfx.height() / 2 + 30);

  Serial.printf("Display ready: %d x %d\n", gfx.width(), gfx.height());
}

void loop() {
  static uint32_t lastBeat = 0;
  int32_t x, y;
  if (gfx.getTouch(&x, &y)) {
    gfx.fillCircle(x, y, 8, TFT_ORANGE);
    Serial.printf("touch %ld,%ld\n", (long)x, (long)y);
  }
  if (millis() - lastBeat >= 5000) {
    lastBeat = millis();
    Serial.printf("alive, uptime %lus\n", (unsigned long)(millis() / 1000));
  }
  delay(10);
}
