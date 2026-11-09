#!/usr/bin/env python3
"""Capture the CrowPanel frame buffer over serial and save it as a PNG.

Usage: tools/screenshot.py --port /dev/cu.wchusbserial210 [--baud 921600] out.png
"""
import argparse
import re
import sys
import time

import serial
from PIL import Image

HEADER = re.compile(rb"SHOT (\d+) (\d+) RGB565RLE (\d+)\n")


def read_header(ser, timeout):
    """Scan the stream (log lines may precede it) until the SHOT header line."""
    deadline = time.time() + timeout
    buf = b""
    while time.time() < deadline:
        buf += ser.read(max(1, ser.in_waiting))
        m = HEADER.search(buf)
        if m:
            return int(m[1]), int(m[2]), int(m[3]), buf[m.end():]
    sys.exit("timed out waiting for SHOT header")


def decode(payload, w, h):
    px = bytearray()
    for i in range(0, len(payload), 3):
        v = (payload[i] << 8) | payload[i + 1]  # readRect yields byte-swapped RGB565
        r, g, b = (v >> 11) & 31, (v >> 5) & 63, v & 31
        rgb = bytes(((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)))
        px += rgb * payload[i + 2]
    if len(px) != w * h * 3:
        sys.exit(f"decoded {len(px) // 3} pixels, expected {w * h}")
    return Image.frombytes("RGB", (w, h), bytes(px))


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--port", required=True)
    ap.add_argument("--baud", type=int, default=921600)
    ap.add_argument("--timeout", type=float, default=15.0)
    ap.add_argument("out")
    a = ap.parse_args()

    t0 = time.time()
    with serial.Serial(a.port, a.baud, timeout=1) as ser:
        ser.reset_input_buffer()
        ser.write(b"shot\n")
        w, h, n, data = read_header(ser, a.timeout)
        deadline = time.time() + a.timeout
        while len(data) < n + 5 and time.time() < deadline:
            data += ser.read(max(1, min(ser.in_waiting, n + 5 - len(data))))
    if len(data) < n + 5 or data[n:n + 5] != b"\nEND\n":
        sys.exit("incomplete or corrupt capture (missing END marker)")
    decode(data[:n], w, h).save(a.out)
    print(f"saved {a.out} ({w}x{h}, {n} bytes) in {time.time() - t0:.2f} s")


if __name__ == "__main__":
    main()
