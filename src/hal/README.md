# src/hal

Hardware abstraction layer. The only code that touches Arduino, ESP-IDF or LovyanGFX directly.

Belongs here: the LovyanGFX device config (`crowpanel7_lgfx.h`), `Display`, `Touch`, `Storage` (NVS
implementation of `IStorage`), `Clock`, `Log`, `Scanout` (vsync hook and scan-line readout for the
RGB panel, see `docs/rendering-notes.md`), and `AiWorker` (FreeRTOS task on core 0).

Does not belong here: game rules, scoring, or screen logic.
