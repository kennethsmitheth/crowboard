# src/ui

Rendering and widgets: `Theme`, `BoardGeometry`, `DirtyRects`, `Renderer`, `Sprites`, `BoardView`,
`Hud`, `Animator`, `Widgets` (with `WidgetCore`), `TapDetector` and `Tween`.

`DirtyRects` (header-only, host-tested) tracks dirty board cells and HUD rectangles. `Renderer` paints
only those through registered `CellPainter`/`RectPainter` callbacks, timed against the panel's
scan-out (`hal/Scanout`). `RenderBench` holds the timing experiments and the `anim` test animation
behind serial commands, and `WidgetsDemo` the `widgets` demo. Measurements and the chosen strategy:
`docs/rendering-notes.md`.

Renders `game/` types (for example a `Board`) but never mutates game state.
