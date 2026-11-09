# src/app

Scenes and scene management. `Nav.h` maps `SceneId` to the statically allocated scenes. `Scene.h` is
the scene interface, `SceneManager` and `SceneStack` run the stack and its pending transitions, and
`TouchGate` drops touches that straddle a transition. `GameConfig` holds the menu choices that Play
reads, `PlayLogic` the selection rules, `GameOverText` the Game Over text, and `Version.h` the firmware
version. `scenes/` has Splash, MainMenu, GameSetup, Play, GameOver, HighScores and Settings.

This is the only layer that wires together `hal/`, `game/`, `ui/` and `services/`.
