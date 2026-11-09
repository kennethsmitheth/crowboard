# test

Native unit tests (Unity) for `game/`, `services/` and the header-only parts of `ui/`, run on the host
with `pio test -e native`. One directory per suite (`test_board`, `test_movegen`, `test_input`,
`test_dirty`, ...), each with its own `main()`.
