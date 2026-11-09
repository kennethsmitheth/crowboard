# Contributing

## Build, flash, test

```
pio run                # build firmware (crowpanel7)
pio run -t upload      # flash over USB (CH340, /dev/cu.wchusbserial210)
pio device monitor     # serial output at 115200 baud
pio test -e native     # host-side Unity tests for src/game and src/services
```

## Architecture rule

`src/game` and `src/services` must stay free of Arduino and ESP-IDF includes (no `Arduino.h`,
`esp_*.h`, `freertos/*`). They build on the host for `pio test -e native`. Hardware access goes
through interfaces injected from `src/hal`. See `docs/ARCHITECTURE.md` section 3.

## Formatting

Sources are formatted with clang-format using the repo's `.clang-format`. Editor settings live in
`.editorconfig`.

```
tools/format.sh          # rewrite src/**/*.{h,cpp} and test/**/*.cpp in place
tools/format.sh --check  # report differences only; non-zero exit if any
```

Install clang-format with `brew install clang-format`, or with
`~/.platformio/penv/bin/python -m pip install clang-format`. The script finds either.

## Branches

Name branches `issue-<n>-<slug>`, for example `issue-5-clang-format`.

## Pull requests

- Every PR must keep CI green. CI builds `crowpanel7`, runs `pio test -e native`, and must pass
  before merge.
- Run `tools/format.sh --check` before pushing.
- Link the issue in the PR description.

## Commit messages

- Short subject line in the imperative mood, about 72 characters or fewer.
- A blank line, then a body that explains what changed and why.
- A final line `Refs #<n>` pointing at the issue.

```
Add clang-format config and format existing sources

Google base style with 2-space indent and 100 columns. Include order is
left alone because Arduino headers depend on it.

Refs #5
```
