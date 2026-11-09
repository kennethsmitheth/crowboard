# tools

Host-side helper scripts.

## screenshot.py

Captures the panel's 800x480 frame buffer over serial and writes a PNG.

```
pip install -r tools/requirements.txt
python3 tools/screenshot.py --port /dev/cu.wchusbserial210 out.png   # --baud defaults to 921600
```

Close `pio device monitor` first; only one program can hold the port. A capture takes well under a
second on a flat UI.

Protocol (firmware side: `src/hal/SerialCmd.cpp`): the host sends `shot\n`. The device replies
`SHOT <w> <h> RGB565RLE <bytes>\n`, then exactly `<bytes>` of payload, then `\nEND\n`. The payload is
run-length encoded: 3-byte tokens of (RGB565 high byte, RGB565 low byte, run length 1..255). Log and
diagnostics lines are muted while streaming. Other commands: `heap` (print the diagnostics line now)
and `help`.
