# src/ui

Rendering and widgets: `Theme`, `DirtyRects`, `Renderer`, `Sprites`, `BoardView`, `Hud`, `Animator`,
`Widgets`.

`DirtyRects` (header-only, host-tested) tracks dirty board cells and HUD rectangles; `Renderer`
paints only those through registered `CellPainter`/`RectPainter` callbacks, timed against the
panel's scan-out (`hal/Scanout`). `RenderBench` holds the timing experiments and the `anim` test
animation behind serial commands. Measurements and the chosen strategy: `docs/rendering-notes.md`.

Renders `game/` types (for example a `Board`) but never mutates game state. Talks to the display
through `hal/`.
