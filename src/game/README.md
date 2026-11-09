# src/game

Pure C++17 checkers engine: `Board`, `Move`, `MoveGen`, `Rules`, `Game`, `Eval`, `Search`,
`Zobrist`/`TT`, `Difficulty`, `Notation`.

Must not include Arduino or ESP-IDF headers, so it builds in the host `native` environment for tests.
Hardware-dependent needs (time, storage) are injected from `hal/`.
