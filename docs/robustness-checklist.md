# Robustness checklist (issue #41)

Fault-injection checks for the real CrowPanel 7.0". Open a serial monitor at 921600 baud
(`pio device monitor`). The injection commands need `-DCROWBOARD_DEBUG_CMDS`, which the
`crowpanel7` env sets in `platformio.ini`; remove that flag for a release build and the commands
disappear. Pass criterion for every row: the board behaves as listed and never reboot-loops.

## Settings

- Task watchdog: the Arduino loop task is subscribed with a 10 s timeout
  (`hal::kWatchdogTimeoutMs`, `src/hal/Watchdog.h`), panic on expiry. `loop()` feeds it once per
  iteration. `bench` and `anim` call `hal::feedWatchdog()` per sample / frame (`RenderBench.cpp`).
  Idle-task checking on core 0 keeps the sdkconfig default.
- Reset reason is logged at every boot; a core dump left by a panic or WDT is summarised once and
  erased (`src/hal/BootInfo.cpp`).
- Diag line (every 5 s) ends with `stack min free N | touch: ok|absent | storage: nvs|RAM-ONLY`.

## Faults

| # | Fault | How to inject | Expected behaviour and log lines |
|---|-------|---------------|----------------------------------|
| 1 | Normal boot | Power cycle | `[I] boot: reset reason POWERON (1)`, `[I] wdt: loop task watched, timeout 10000 ms`, `[I] touch: ready`; no `coredump:` lines |
| 2 | Touch controller absent | Power off, unplug the touch flex cable from the GT911 connector, power on | `[W] touch: absent (GT911 did not answer on I2C); taps disabled`; display and scenes (splash, menu animations) still render; taps do nothing; Diag line shows `touch: absent`; no reboot. Reconnect only with power off |
| 3 | Corrupt settings blob | Boot normally, then `nvs corrupt settings v1`, then reset (EN button) | `[I] nvs corrupt: flipped last byte of settings/v1 (N bytes)`; after reset the Settings load logs a CRC failure and uses defaults; game boots, saving a setting rewrites a good blob |
| 4 | Corrupt other blobs | Same with `nvs corrupt scores v1`, `nvs corrupt stats v1`, `nvs corrupt save v1` | Each falls back to defaults / empty / "no save" (Resume hidden) without crashing |
| 5 | Software panic | `crash` | `[W] crash: aborting on purpose`, panic dump on UART, reboot. Next boot: `[W] boot: reset reason PANIC (4)`, then `[E] coredump: task 'loopTask' pc 0x... cause ...`, `coredump: reason 'abort() was called ...'`, `coredump: backtrace ...`. A second reset shows only `reset reason SW`/`POWERON` and no `coredump:` lines (erased) |
| 6 | Watchdog fires on a hung loop | `crash hang` | `[W] crash: spinning ...`; after ~10 s `Task watchdog got triggered` naming `loopTask`, reboot; next boot `reset reason TASK_WDT (7)` plus a `coredump:` summary. One reboot only, then normal boot |
| 7 | Watchdog does not fire on long commands | `bench`, then `anim beam 60`, `anim vsync 60`, `anim raw 60` (and `slide`) | Each runs to completion ("bench done", "anim done: ...") with no `Task watchdog` message and no reboot; `anim` above 10 s is the key case |
| 8 | Watchdog vs AI load | `ai hard` and `aibench` while `anim beam 30` runs | No WDT reboot; loop keeps ticking (Diag `loops/s` > 0) |
| 9 | NVS init failure | Needs a damaged NVS partition: `esptool.py --port <p> write_flash 0x310000 garbage.bin` with a 64 KB file of random bytes (a plain erase is handled by the existing erase-and-reinit path and will NOT trigger this) | If init still succeeds after the automatic erase there is nothing to see, which is the intended recovery. Otherwise `[W] storage: NVS init failed; using RAM storage, settings/scores/saves are NOT persisted`, Diag shows `storage: RAM-ONLY`; changing a setting, finishing a game and saving still work until reboot; nothing persists across reset |
| 10 | Reset reasons | Press EN: `SW`/`EXT`; unplug and replug USB: `POWERON`; brown-out by a weak supply: `BROWNOUT` | Text matches; only abnormal reasons log at `[W]` |

## Host-verified (no board needed)

`pio test -e native`: `ForwardingStorage` delegation, NVS-fallback selection (`test_storage`),
`TapDetector` `minPressMs` and `maxTapsPerSecond` (`test_input`).

## Notes

- Phantom taps (#45): `TapConfig::minPressMs` and `maxTapsPerSecond` default to 0 (off). Both
  gate only the `Tap` event; Down/Up/Move are unchanged. Not wired to any value yet.
- A touch controller that is present at boot but stops answering later is not detected; `getTouch`
  just returns no touch.
