# test

Native unit tests (Unity) for `game/`, `services/`, the header-only parts of `ui/` and the scene logic
in `app/`, run on the host with `pio test -e native`. One directory per suite (`test_board`,
`test_movegen`, `test_input`, `test_dirty`, ...), each with its own `main()`. `common/perft.h` holds the
shared perft helpers.

`test_rules` runs perft to depth 7 by default; depth 8 needs `-DPERFT_DEEP`. `test_selfplay` plays
Hard against Medium on the host (about 8 seconds, see `docs/ai-notes.md`).
