# AI notes (issue #18)

Difficulty profiles live in `src/game/Difficulty.h` (`kDifficultyProfiles`):

| Profile | Budget | Depth cap | TT | Move choice |
|---|---|---|---|---|
| Easy | 150 ms (safety cap) | 2 | no | every root move scored at depth 2, random pick among the best 3 (`AiRng`, seeded by the caller) |
| Medium | 500 ms | 64 | no | best move of iterative deepening |
| Hard | 2500 ms | 64 | 1 MB in PSRAM (87,381 entries fit, 65,536 used: power of two) | best move of iterative deepening |

## Device benchmark

Serial command `aibench [check N]` (hal/SerialCmd.cpp) runs on the AI worker (core 0) while the
UI loop keeps running on core 1. Positions: the initial position and the mid-game position
`B:W1,3,4,9,10,11,12,14:B18,21,23,24,29,30,31,32` (8 v 8, ply 20 of a host self-play game).
Per position: fixed depth 6 without and with the TT (no budget), then each profile. ESP32-S3
240 MHz, Arduino core 3.3, -Os build. All numbers below were measured on the board.

Before (main as of 49427c8: eval mobility generates the moves of both sides at every leaf,
Searcher in PSRAM, -Os; Hard row measured with the new 1 MB TT):

| Position | Run | Depth | Nodes | ms | nodes/s |
|---|---|---|---|---|---|
| initial | d6 | 6 | 22,214 | 743 | 29,897 |
| initial | d6 + TT | 6 | 18,800 | 607 | 30,971 |
| initial | Easy | 2 | 86 | 5 | - |
| initial | Medium | 5 | 15,360 | 518 | 29,652 |
| initial | Hard (TT) | 7 | 78,848 | 2523 | 31,251 |
| initial | Hard, no TT (`ai hard` on main) | 6 | 74,752 | 2511 | ~29,800 |
| mid | d6 | 6 | 46,401 | 1362 | 34,068 |
| mid | d6 + TT | 6 | 36,917 | 979 | 37,708 |
| mid | Medium | 5 | 17,408 | 527 | 33,032 |
| mid | Hard (TT) | 6 | 98,304 | 2510 | 39,164 |

After (final configuration, the committed defaults):

| Position | Run | Depth | Nodes | ms | nodes/s |
|---|---|---|---|---|---|
| initial | d6 | 6 | 22,214 | 286 | 77,671 |
| initial | d6 + TT | 6 | 18,800 | 262 | 71,755 |
| initial | Easy | 2 | 86 | 2 | - |
| initial | Medium | 6 | 38,912 | 500 | 77,824 |
| initial | Hard (TT) | 8 | 179,200 | 2512 | 71,337 |
| mid | d6 | 6 | 46,401 | 534 | 86,893 |
| mid | d6 + TT | 6 | 36,917 | 449 | 82,220 |
| mid | Easy | 2 | 230 | 3 | 76,666 |
| mid | Medium | 5 | 44,032 | 506 | 87,019 |
| mid | Hard (TT) | 7 | 207,872 | 2500 | 83,148 |

Search speed went from ~30k to ~78-87k nodes/s (2.6x) with identical node counts at fixed depth;
Hard gained one ply (initial 7 -> 8, mid 6 -> 7) and two plies over main without a TT. During
every run the UI loop stayed at 99 iterations/s (its 10 ms delay is the limit), worst loop
11.0 ms; internal heap 309,828 bytes free (min 309,712), worker stack 16,968 of 24,576 bytes
still free at the deepest point.

## Experiments

All measured with `aibench` on the device, one change at a time.

* **(a) Searcher in internal RAM vs PSRAM** (`-DAI_SEARCHER_INTERNAL=1`), measured on the old
  code: identical speed (initial d6 741 vs 743 ms, mid d6 1360 vs 1362 ms) while the internal heap
  dropped from 309,828 to 178,752 bytes. The working set (a few per-ply move lists, history)
  fits in the data cache. Kept in PSRAM. Not re-measured after the speed-ups (the board was
  disconnected mid-run); see follow-ups.
* **(b) Eval mobility weight 0 vs 2** (`-DAI_EVAL_MOBILITY=0`), old code: 54.6k vs 29.9k
  nodes/s at the initial position, Hard depth 7 vs 7, Medium 6 vs 5. Rather than dropping the
  term, mobility is now counted exactly from bitboards (`countLegalMoves()`, captures generated
  only for a side that has one), and a depth-0 node without a capture is evaluated without
  generating its moves at all. With the weight kept at 2 this is faster (62.6k nodes/s at -Os)
  than the old code with mobility off, and evaluation is unchanged (same scores, same nodes).
* **(c) 1 MB TT in PSRAM**: fixed depth 6 needs 15% (initial) / 20% (mid) fewer nodes; nodes/s
  drops ~6-8% (PSRAM probes); within the Hard budget it is worth one ply at the initial position
  on the old code (6 -> 7). Kept for Hard. The worker clears it on `AiRequest::newGame` and when
  `forcedCapture` differs from the previous TT search (`AiTable`); the clear is not charged to
  the budget.
* **(d) Stop-check interval 1024 vs 4096** (`aibench check 4096`): no measurable speed
  difference (initial d6 286 ms both, Hard 179,200 vs 180,224 nodes), but budget overshoot grows
  from ~0-16 ms to ~12-28 ms. `millis()` every 1024 nodes costs nothing measurable. Kept 1024.
* **(e) -O2 for Search/MoveGen/Eval** (`#pragma GCC optimize("O2")` under `__XTENSA__`; a global
  `-O2` in build flags is overridden by the framework's `-Os`): 62.6k -> 77.7k nodes/s (initial),
  68.8k -> 86.9k (mid). Kept.

Chosen configuration: Searcher in PSRAM, 1 MB TT in PSRAM for Hard, mobility 2 with bitboard
counting, check interval 1024, -O2 on the hot files. It gives the best Hard depth measured and
keeps the internal heap at ~310 KB free and the UI loop at 99 iterations/s.

## Host self-play

`test/test_selfplay` (part of `pio test -e native`, ~8 s): Hard (depth 6 + TT, cleared per game)
versus Medium (depth 3), both depth-capped so the run is deterministic; 25 two-ply openings chosen
with `AiRng`, each played with both colours. Result: 50 games, 40 wins, 10 draws, 0 losses,
score 90% (threshold 70%). The depth gap of three plies matches the device (Medium 5-6, Hard
7-8 plus TT). With a two-ply gap (depth 5 + TT vs 3) the score was 68% (21 W 26 D 3 L), so the
test uses the three-ply gap; no eval weights were changed. `-DSELFPLAY_LONG` plays 100 games at
depth 7 + TT vs 5 (not run yet).

## Follow-ups (not measured)

* Searcher in internal RAM with the faster code (the 131 KB would leave ~179 KB internal free).
* The `SELFPLAY_LONG` run.
* Easy's 150 ms cap is never reached (2-5 ms); it is only a safety net.
