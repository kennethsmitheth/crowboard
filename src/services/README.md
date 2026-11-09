# src/services

Persistence and bookkeeping that sits on top of `game/`: `Scoring`, `HighScores`, `Settings`,
`SaveGame`, `Stats`.

Must not include Arduino or ESP-IDF headers. Storage is reached through the `IStorage` interface
from `hal/`, so these modules build and test on the host.
