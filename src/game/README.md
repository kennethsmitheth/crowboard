# src/game

Pure C++17 checkers engine: `Board`, `Move`, `MoveList`, `MoveGen`, `Apply`, `Game` (with its `Rules`
struct), `Eval`, `Search`, `TT`, `Zobrist`, `Difficulty`, `Notation`. `AiRequest` and `AiSync` are the
request and result types shared with the device worker in `hal/`.

Must not include Arduino or ESP-IDF headers, so it builds in the host `native` environment for tests.
Hardware-dependent needs (time, storage) are injected from `hal/`.
