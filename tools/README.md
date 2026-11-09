# tools

Host-side helper scripts.

## screenshot.py

Captures the panel's 800x480 frame buffer over serial and writes a PNG.

```
pip install -r tools/requirements.txt
python3 tools/screenshot.py --port /dev/cu.wchusbserial210 out.png   # --baud defaults to 921600
```

Options: `--port` (required), `--baud` (default 921600), `--timeout` (seconds, default 15), and the
output path. Close `pio device monitor` first; only one program can hold the port. A capture takes well
under a second on a flat UI.

Protocol (firmware side: `src/hal/SerialCmd.cpp`): the host sends `shot\n`. The device replies
`SHOT <w> <h> RGB565RLE <bytes>\n`, then exactly `<bytes>` of payload, then `\nEND\n`. The payload is
run-length encoded: 3-byte tokens of (RGB565 high byte, RGB565 low byte, run length 1..255). Log and
diagnostics lines are muted while streaming. Other commands: `heap` (print the diagnostics line now),
`help`, and the rendering experiments `bench`, `anim [beam|vsync|raw] [seconds]` and `render`
(`docs/rendering-notes.md`). `anim` and `bench` block the firmware's main loop while they run, so take
screenshots before or after, not during.

## format.sh

Runs clang-format over `src/**/*.{h,cpp}` and `test/**/*.{h,cpp}`. `tools/format.sh` rewrites the files
in place; `tools/format.sh --check` reports differences and exits non-zero if there are any. It finds
clang-format on `PATH`, in the PlatformIO Python environment, or in Homebrew's prefix. See
`CONTRIBUTING.md`.
