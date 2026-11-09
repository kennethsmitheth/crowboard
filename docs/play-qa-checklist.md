# Play scene manual QA checklist

Run on the panel after every change to the Play scene (`src/app/scenes/PlayScene.*`,
`src/app/PlayLogic.h`). Square numbers follow ARCHITECTURE §6 (top-right is square 4,
bottom-left is 29). The pure selection rules are also covered by `test/test_playlogic`.

How to get there until the menus exist: the boot fallback opens Play directly (Human vs
Computer, Medium, human = Black, moves first; an existing autosave is resumed). Two-player items
need Game Setup (#34) or a temporary `mode = Mode::TwoPlayers` default in `src/app/GameConfig.h`.
Menu returns to the Main Menu (#33); while that scene is missing, Menu starts a new game.

## Board and HUD

- [ ] Board fills x 0-479, sidebar x 480-799; no flicker or tearing while idle.
- [ ] Sidebar shows both player rows (initials, piece count, captured count), "<initials> to
      move" for the side to move, its count-up clock (appears after the first second), and the
      Undo / Hint / Menu buttons. Hint is greyed out and does nothing.
- [ ] Vs computer the computer's row reads "CPU"; the human's row shows P1 initials.
- [ ] The clock of the side to move advances once per second and stops when the game ends.

## Selection (ARCHITECTURE §8)

- [ ] Tapping an own piece with a legal move highlights it and shows dots on its landing squares.
- [ ] Tapping an own piece without a legal move (e.g. 29 at the start) does nothing.
- [ ] Tapping an enemy piece, an empty dark square or a light square clears the selection.
- [ ] Tapping the selected piece again deselects it.
- [ ] Tapping another movable own piece switches the selection to it.
- [ ] Tapping a dot plays the move instantly; the last-move outline marks from and to.
- [ ] Taps on the board are ignored while "Computer thinking..." is shown.

## Captures (§5)

- [ ] Forced capture cue: when a capture is available every capturing piece gets a ring, the
      status line reads "Capture is forced" and only those pieces can be selected.
- [ ] With forcedCapture off in Settings, non-capturing moves are also selectable when a
      capture exists, and no ring / forced status is shown unless every move is a capture.
- [ ] Multi-jump continuation: after the first landing of a double jump the landing square is
      outlined, the next landing gets a dot and the move is not played yet. The second tap plays
      the whole sequence and removes both captured pieces.
- [ ] Two sequences sharing the first landing: after the shared landing both next landings show
      dots; the chosen one decides which pieces are captured.
- [ ] Tapping elsewhere during a partial jump path clears the path and the selection; the board
      is unchanged.
- [ ] A piece is never jumped twice in one sequence (no dot offered back over a captured piece).

## Promotion (§5)

- [ ] A man reaching the far row becomes a king (crown sprite).
- [ ] Promotion ends the turn even when the new king could keep jumping: the move resolves on the
      promotion square and the turn passes.
- [ ] Kings move and capture backwards; one square at a time (no flying kings).

## AI (§9)

- [ ] After the human moves, "Computer thinking..." appears and the AI replies within its budget
      (Easy ~0.15 s, Medium ~0.5 s, Hard ~2.5 s). The UI and clock keep running meanwhile.
- [ ] The AI's own forced captures and multi-jumps are applied correctly.
- [ ] With the human playing White (config `humanSide = Side::White`) the computer opens.

## Undo

- [ ] Vs computer on the human's turn: Undo takes back the computer's reply and the human's move.
- [ ] Undo during AI thinking: the request is cancelled, only the human's last move is undone,
      no late AI move appears afterwards, and it is the human's turn again.
- [ ] Undo is disabled before the human's first move and after the game is over.
- [ ] Two players: Undo takes back exactly one ply.
- [ ] Undo restores captured pieces, piece counts, captured counts and demotes an undone king.

## Autosave and resume (§10)

- [ ] Make a few moves, power-cycle the panel: the game resumes in the same position, same side
      to move, same clocks (to the second of the last move), same mode and difficulty.
- [ ] Power-cycle during "Computer thinking...": on resume the computer moves again.
- [ ] Menu during a game autosaves; resuming continues it.
- [ ] After a finished game the save is erased: the next boot starts a fresh game.

## Game end (§5)

- [ ] Capturing the last enemy piece ends the game: "You win!" / "Computer wins" (vs computer)
      or "<initials> wins!" (two players) in the status line; board input stops; Undo disabled.
- [ ] A side with pieces but no legal move loses (block the last pieces).
- [ ] 40-move draw: 80 plies without a capture or a man move (e.g. two-player king shuffling)
      shows "Draw: 40-move rule".
- [ ] Repetition draw: the same position with the same side to move three times shows
      "Draw: repetition".
- [ ] When Game Over (#36) exists, the game end switches to it and its summary matches the game.
- [ ] Vs computer stats count the result once (Stats: played / won / lost / drawn); two-player
      games increment the two-player count.
