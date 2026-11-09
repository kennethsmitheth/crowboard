# src/hal

Hardware abstraction layer. The only code that touches Arduino, ESP-IDF or LovyanGFX directly.

Belongs here: the LovyanGFX device config (`crowpanel7_lgfx.h`), `Touch`, `NvsStorage` (the NVS
implementation of `services/IStorage.h`), `StorageSelector` and `ForwardingStorage` (the choice of NVS
or a RAM fallback), `Scanout` (vsync hook and scan-line readout for the RGB panel, see
`docs/rendering-notes.md`), `AiWorker` (FreeRTOS task on core 0), `SerialCmd`, `Diag`, `Log.h`,
`BootInfo` and `Watchdog`.

Does not belong here: game rules, scoring, or screen logic.
