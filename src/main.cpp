// Elecrow CrowPanel ESP32 7.0" checkers: hardware init and the main loop.
// The loop polls touch and runs the current scene through the SceneManager.
#include <Arduino.h>
#include "hal/Log.h"
#include "hal/AiWorker.h"
#include "hal/Diag.h"
#include "hal/SerialCmd.h"
#include "hal/crowpanel7_lgfx.h"
#include "ui/Animator.h"
#include "ui/BoardView.h"
#include "ui/Hud.h"
#include "ui/Sprites.h"
#include "hal/Touch.h"
#include "ui/RenderBench.h"
#include "ui/Renderer.h"
#include "hal/NvsStorage.h"
#include "services/HighScores.h"
#include "services/SaveGame.h"
#include "services/Settings.h"
#include "services/Stats.h"
#include "app/Nav.h"
#include "app/SceneManager.h"
#include "app/GameConfig.h"
#include "app/scenes/MainMenuScene.h"
#include "app/scenes/SplashScene.h"
#include "app/scenes/HighScoresScene.h"
#include "app/scenes/SettingsScene.h"
#include "app/scenes/PlayScene.h"

static LGFX gfx;
static Diag diag;
static checkers::hal::Touch touch;
static SerialCmd cmd(gfx, diag);
static checkers::ui::Sprites sprites;
static checkers::hal::AiWorker ai;
static checkers::ui::Renderer renderer(gfx);
static checkers::ui::BoardView boardView(renderer, sprites);
static checkers::ui::Hud hud(renderer);
static checkers::ui::Animator animator(renderer, sprites, boardView);
static checkers::ui::RenderBench bench(gfx, renderer);  // serial: bench, anim, render

// ---- scenes -----------------------------------------------------------------------------------
static NvsStorage storage;
static Settings settings(storage);
static checkers::HighScores highScores(storage);
static checkers::Stats stats(storage);
static checkers::SaveGame saveGame(storage);
static checkers::app::SceneManager scenes;
static checkers::app::Nav nav(scenes);
static checkers::app::GameConfig gameConfig;
static checkers::app::SceneContext ctx{gfx,      renderer,   sprites, ai,       scenes,    nav,
                                       settings, highScores, stats,   saveGame, gameConfig};
static checkers::app::SplashScene splashScene;
static checkers::app::MainMenuScene mainMenuScene;
static checkers::app::HighScoresScene highScoresScene;
static checkers::app::SettingsScene settingsScene;
static checkers::app::PlayScene playScene(renderer, sprites);
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
  if (!storage.begin()) LOG_E("NVS init failed; settings and scores will not persist");
  settings.load();
  highScores.load();
  stats.load();
  LOG_I("Settings: brightness %u%% forcedCapture %u theme %u difficulty %u save %s",
        settings.data().brightness, settings.data().forcedCapture, settings.data().theme,
        settings.data().defaultDifficulty, saveGame.exists() ? "present" : "none");
  gfx.setBrightness(static_cast<uint8_t>(settings.data().brightness * 255u / 100u));
  touch.begin(gfx);
  renderer.begin();  // scan-out hook for beam-synced flushes
  bench.attachAnimator(&animator);
  bench.attachView(&boardView, &hud);  // serial `view 0|1|2` installs these painters on demand
  cmd.setHandler(
      [](const char* line, void* ctx) {
        return static_cast<checkers::ui::RenderBench*>(ctx)->handleCommand(line);
      },
      &bench);

  LOG_I("Display ready: %d x %d", gfx.width(), gfx.height());

  // ---- scenes ----
  // Register real scenes here as they land: nav.set(checkers::app::SceneId::X, &xScene);
  nav.set(checkers::app::SceneId::Splash, &splashScene);
  nav.set(checkers::app::SceneId::MainMenu, &mainMenuScene);
  nav.set(checkers::app::SceneId::HighScores, &highScoresScene);
  nav.set(checkers::app::SceneId::Settings, &settingsScene);
  nav.set(checkers::app::SceneId::Play, &playScene);
  nav.go(checkers::app::SceneId::Splash);
}

void loop() {
  const uint32_t frameStart = millis();
  diag.tick();
  cmd.tick();
  touch.poll(frameStart);

  // ---- scenes ----
  if (bench.widgetsDemo().active()) {
    // The serial `widgets` demo owns input and the screen until reboot.
    checkers::ui::TouchEvent ev;
    while (touch.pop(ev)) bench.widgetsDemo().handle(ev);
    renderer.flush();
  } else if (bench.slideRunning()) {
    bench.tickSlide(frameStart);  // serial `slide` demo owns the screen until it finishes
  } else {
    scenes.tick(frameStart, touch, ctx);
  }

  // Frame pacing: sleep the rest of the frame (at least 1 ms) so idle loops run at ~60 Hz.
  const uint32_t spent = millis() - frameStart;
  delay(spent < kFrameMs ? kFrameMs - spent : 1);
}
