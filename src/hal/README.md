# src/hal

Hardware abstraction layer. The only code that touches Arduino, ESP-IDF or LovyanGFX directly.

Belongs here: the LovyanGFX device config (`crowpanel7_lgfx.h`), `Display`, `Touch`, `Storage` (NVS
implementation of `IStorage`), `Clock`, `Log`, and `AiWorker` (FreeRTOS task on core 0).

Does not belong here: game rules, scoring, or screen logic.
