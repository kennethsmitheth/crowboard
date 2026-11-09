# src/services

Persistence and bookkeeping that sits on top of `game/`: `Scoring`, `HighScores`, `Settings`,
`SaveGame`, `Stats`. Also the storage layer: the `IStorage` interface, `RamStorage` (tests and the
NVS fallback), the versioned blob format in `Blob` and `Crc32`, and the shared enums in `GameTypes`.

Must not include Arduino or ESP-IDF headers. Storage is reached through `IStorage`, which the NVS
implementation in `hal/` provides, so these modules build and test on the host.
