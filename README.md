# crowboard

Firmware playground for the Elecrow CrowPanel ESP32 7.0" HMI (ESP32-S3-WROOM-1-N4R8, 800x480 RGB TFT, GT911 capacitive touch). Current state: a LovyanGFX hello-world that draws a greeting and echoes touches. Goal: a touch-driven checkers game.

## Build and flash

```
pio run                # build
pio run -t upload      # flash over USB (CH340, /dev/cu.wchusbserial210)
pio device monitor     # serial output at 115200 baud
pio test -e native     # host-side Unity tests for src/game and src/services
```

## Hardware notes

- Display pin map and timing live in `src/crowpanel7_lgfx.h`.
- PSRAM is required for the frame buffer and is configured in `platformio.ini` (memory_type `qio_opi`, 4MB flash, `default.csv` partitions).
