# crowboard

[![CI](https://github.com/kennethsmitheth/crowboard/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/kennethsmitheth/crowboard/actions/workflows/ci.yml)

Firmware playground for the Elecrow CrowPanel ESP32 7.0" HMI (ESP32-S3-WROOM-1-N4R8, 800x480 RGB TFT, GT911 capacitive touch). Current state: a LovyanGFX hello-world that draws a greeting and echoes touches. Goal: a touch-driven checkers game.

## Build and flash

```
pio run                # build
pio run -t upload      # flash over USB (CH340, /dev/cu.wchusbserial210)
pio device monitor     # serial output at 921600 baud
pio test -e native     # host-side Unity tests for src/game and src/services
```

## Screenshots

The firmware accepts line commands on the serial port (921600 baud): `shot`, `heap`, `help`.
`tools/screenshot.py` uses `shot` to save what the panel shows as a PNG:

```
pip install -r tools/requirements.txt
python3 tools/screenshot.py --port /dev/cu.wchusbserial210 out.png
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
- PSRAM is required for the frame buffer and is configured in `platformio.ini` (memory_type `qio_opi`, 4MB flash, `default.csv` partitions).
