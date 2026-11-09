// Elecrow CrowPanel ESP32 7.0" checkers: hardware init and the main loop.
// The loop polls touch and runs the current scene through the SceneManager.
#include <Arduino.h>
#include "hal/Log.h"
#include "hal/AiWorker.h"
#include "hal/Diag.h"
#include "hal/SerialCmd.h"
#include "hal/crowpanel7_lgfx.h"
#include "ui/BoardView.h"
#include "ui/Hud.h"
#include "ui/Sprites.h"
#include "hal/Touch.h"
#include "ui/RenderBench.h"
#include "ui/Renderer.h"
#include "app/SceneManager.h"
#include "app/scenes/DemoScenes.h"

static LGFX gfx;
static Diag diag;
static checkers::hal::Touch touch;
static SerialCmd cmd(gfx, diag);
static checkers::ui::Sprites sprites;
static checkers::hal::AiWorker ai;
static checkers::ui::Renderer renderer(gfx);
static checkers::ui::BoardView boardView(renderer, sprites);
static checkers::ui::Hud hud(renderer);
static checkers::ui::RenderBench bench(gfx, renderer);  // serial: bench, anim, render

// ---- scenes -----------------------------------------------------------------------------------
static checkers::app::SceneManager scenes;
static checkers::app::SceneContext ctx{gfx, renderer, sprites, ai, scenes};
static checkers::app::DemoSceneA sceneA;
static checkers::app::DemoSceneB sceneB;
static constexpr uint32_t kFrameMs = 16;  // idle loop iterations are padded to this (~60 Hz)

void setup() {
  Serial.begin(921600);
  delay(300);
  LOG_I("CrowPanel 7.0\" hello world");
  LOG_I("PSRAM: %u bytes total, %u free", ESP.getPsramSize(), ESP.getFreePsram());
  LOG_I("Heap:  %u bytes free", ESP.getFreeHeap());

  if (!gfx.init()) {
    LOG_E("gfx.init() failed");
  }
  if (!sprites.begin(gfx)) LOG_E("sprites.begin() failed");
  LOG_I("Sprites: %u bytes, PSRAM free %u", (unsigned)sprites.bytes(), ESP.getFreePsram());
  if (!ai.begin()) LOG_E("ai.begin() failed");
  cmd.attachAi(&ai);
  gfx.setBrightness(255);
  touch.begin(gfx);
  renderer.begin();                    // scan-out hook for beam-synced flushes
  bench.attachView(&boardView, &hud);  // serial `view 0|1|2` installs these painters on demand
  cmd.setHandler(
      [](const char* line, void* ctx) {
        return static_cast<checkers::ui::RenderBench*>(ctx)->handleCommand(line);
      },
      &bench);

  LOG_I("Display ready: %d x %d", gfx.width(), gfx.height());

  // ---- scenes ----
  sceneA.setOther(&sceneB);
  sceneB.setOther(&sceneA);
  scenes.replace(&sceneA);
}

void loop() {
  const uint32_t frameStart = millis();
  diag.tick();
  cmd.tick();
  touch.poll(frameStart);

  // ---- scenes ----
  scenes.tick(frameStart, touch, ctx);

  // Frame pacing: sleep the rest of the frame (at least 1 ms) so idle loops run at ~60 Hz.
  const uint32_t spent = millis() - frameStart;
  delay(spent < kFrameMs ? kFrameMs - spent : 1);
}
