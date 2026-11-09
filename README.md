# crowboard

[![CI](https://github.com/kennethsmitheth/crowboard/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/kennethsmitheth/crowboard/actions/workflows/ci.yml)

Touch-driven checkers for the Elecrow CrowPanel ESP32 7.0" HMI (ESP32-S3-WROOM-1-N4R8, 800x480 RGB
TFT, GT911 capacitive touch). Human vs computer with three difficulty levels, two-player pass-and-play,
high scores, settings and autosave. Progress is tracked in the pinned roadmap issue (#44); the design is
in `docs/ARCHITECTURE.md`.

## Try it on a CrowPanel 7.0"

1. Install PlatformIO Core (`pip install platformio`, or the VS Code extension).
2. Clone this repo and plug the panel in over USB-C (it shows up as a CH340 serial device).
3. Build and flash. The first build downloads the ESP32 toolchain and takes a few minutes.

   ```
   pio run -t upload
   pio device monitor
   ```

   PlatformIO auto-detects the serial port. If it picks the wrong one, add `--upload-port <port>`
   (and `--port <port>` for the monitor), or set `upload_port` in `platformio.ini`.
4. You should see a splash screen, then the main menu. High Scores and Settings work from the menu;
   the Play scene lands on `main` as soon as issue #35 is merged (see #44 for current status).

Hardware verification status: the hardware was available up to the renderer and AI tuning work
(issues #20 and #18, including the mock board screenshots in `docs/images/`). Later commits are
verified by the firmware build and the host test suite only; each closed issue says which it was.
If something misbehaves on the panel, open an issue with the serial log (921600 baud).

## Build and flash

```
pio run                # build
pio run -t upload      # flash over USB (CH340; port auto-detected)
pio device monitor     # serial output at 921600 baud
pio test -e native     # host-side Unity tests for src/game and src/services
```

## Screenshots

The firmware accepts line commands on the serial port (921600 baud): `shot`, `heap`, `help`, plus
the rendering experiments `bench`, `anim [beam|vsync|raw] [seconds]` and `render` described in
`docs/rendering-notes.md`. `tools/screenshot.py` uses `shot` to save what the panel shows as a PNG:

```
pip install -r tools/requirements.txt
python3 tools/screenshot.py --port /dev/cu.wchusbserialXXX out.png
```

Close `pio device monitor` first. A capture takes well under a second. Details in `tools/README.md`.

![hello world](docs/images/hello-world.png)

## Layout

Source follows the layered architecture in `docs/ARCHITECTURE.md` (sections 3 and 14). Dependencies point
downward only.

```
src/
  main.cpp      wiring: hal init, SceneManager, loop
  hal/          LovyanGFX config, Display, Touch, Storage, Clock, Log, AiWorker
  game/         pure C++17 checkers engine (host-testable)
  ui/           Theme, BoardView, Hud, Widgets, Animator
  app/          Scene, SceneManager, scenes/
  services/     Scoring, HighScores, Settings, SaveGame, Stats (host-testable)
test/           native unit tests (Unity)
tools/          host-side helper scripts
docs/           ARCHITECTURE.md
```

Each `src/` subdirectory has a README describing what belongs there.

## Hardware notes

- Display pin map and timing live in `src/hal/crowpanel7_lgfx.h`.
- PSRAM is required for the frame buffer and is configured in `platformio.ini` (memory_type `qio_opi`, 4 MB flash, custom `partitions.csv`: 3 MB app, 64 KB NVS, LittleFS, no OTA).
- Rendering strategy and measurements: `docs/rendering-notes.md`. AI benchmark and difficulty profiles: `docs/ai-notes.md`.
