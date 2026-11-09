# src/ui

Rendering and widgets: `Theme`, `DirtyRects`, `Sprites`, `BoardView`, `Hud`, `Animator`, `Widgets`.

Renders `game/` types (for example a `Board`) but never mutates game state. Talks to the display
through `hal/`.
