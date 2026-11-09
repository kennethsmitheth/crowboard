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
      Undo / Hint / Menu buttons. Hint is greyed out while the computer thinks, while a move
      slides and after the game ends.
- [ ] Vs computer the computer's row reads "CPU"; the human's row shows P1 initials.
- [ ] The clock of the side to move advances once per second and stops when the game ends.

## Two-player (pass and play)

- [ ] Each player row shows its own count-up clock (mm:ss, also at 00:00); the side to move has
      an amber bar and an amber clock, the other clock is dimmed and stands still. The single
      clock in the turn row is not shown. Vs computer keeps the single clock.
- [ ] Switching sides after each move moves the highlight; only the two rows and the turn line
      repaint (no whole-sidebar flicker). Clocks resume correctly after Menu and Resume.
- [ ] Settings has "Flip board in two-player" (default off). Off: the board never turns.
- [ ] On: after each completed move (after the slide, never during it) the board rotates so the
      side to move is at the bottom (Player 1 / dark at the bottom on its turn). Taps hit the
      squares shown, including selection dots, last-move outline and forced-capture rings.
- [ ] While flipped, a slide and a multi-jump follow the flipped square positions; the capture
      fade and promotion pulse sit on the right squares; no residue after the flip.
- [ ] Undo flips the board back to match the side to move. Resuming a saved two-player game
      shows the correct orientation. Vs computer never flips, whatever the setting.
- [ ] Game over in two-player: the status line and the Game Over headline use the winner's
      initials (P1 initials when dark wins, P2 initials when ivory wins).

## Selection (ARCHITECTURE §8)

- [ ] Tapping an own piece with a legal move highlights it and shows dots on its landing squares.
- [ ] Tapping an own piece without a legal move (e.g. 29 at the start) does nothing.
- [ ] Tapping an enemy piece, an empty dark square or a light square clears the selection.
- [ ] Tapping the selected piece again deselects it.
- [ ] Tapping another movable own piece switches the selection to it.
- [ ] Tapping a dot plays the move: the piece slides to the landing square (origin square empty
      while it moves), then the last-move outline marks from and to.
- [ ] Taps on the board are ignored while "Computer thinking..." is shown.
- [ ] Taps on the board are ignored while a piece is sliding; the HUD buttons still react.

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

## Animation (§7)

- [ ] A three-jump capture slides hop by hop; the captured pieces shrink away right after the
      slide ends, with no residue pixels on the squares or along the path.
- [ ] The board shows the old position until the slide ends, then updates in one step (no frame
      with the piece on both squares); piece counts and clocks keep updating during the slide.
- [ ] The computer's moves slide the same way; an AI reply is never applied mid-slide.
- [ ] The fade and the promotion pulse do not block taps: select a piece during the fade.
- [ ] Undo during a slide: the animation stops at once, the move is applied and then undone as
      usual (vs computer on the human's move: back to the human's turn before it); no residue.
- [ ] Menu during a slide leaves the scene without a stray piece or autosave glitch.
- [ ] Returning from another scene (Menu, Game Over, stats) the slide still draws (overlay).

## Promotion (§5)

- [ ] A man reaching the far row becomes a king (crown sprite) and an amber ring pulses on the
      square.
- [ ] Promotion ends the turn even when the new king could keep jumping: the move resolves on the
      promotion square and the turn passes.
- [ ] Kings move and capture backwards; one square at a time (no flying kings).

## AI (§9)

- [ ] After the human moves, "Computer thinking..." appears and the AI replies within its budget
      (Easy ~0.15 s, Medium ~0.5 s, Hard ~2.5 s). The UI and clock keep running meanwhile.
- [ ] The AI's own forced captures and multi-jumps are applied correctly.
- [ ] With the human playing White (config `humanSide = Side::White`) the computer opens.

## Hint (#39)

- [ ] On the human's turn Hint is enabled; vs computer it is greyed out during "Computer
      thinking...", during a slide and after the game ends. Two players: enabled on either side's
      turn.
- [ ] Tapping Hint shows "Thinking of a hint..." for about 0.3 s; taps on the board still select
      pieces and the HUD buttons still react meanwhile. The clock keeps running.
- [ ] The suggestion appears as a cyan outline on its from and to squares (distinct from the
      olive last-move outline) and disappears after about 2 s.
- [ ] Tapping anywhere on the board or the sidebar removes a shown hint at once; tapping Hint
      again while it is shown only removes it.
- [ ] Playing a move while the hint is still thinking (possible with a fast tap) leaves no hint
      on the new position: no late outline appears.
- [ ] Undo removes a shown hint; the Undo of the computer's reply does not show a stale hint.
- [ ] The suggested move is legal and is a good move for the difficulty (on Hard it matches the
      computer's choice in most positions; the 0.3 s budget may pick a different move).
- [ ] Forced capture on: when a capture is available the hint suggests a capture.
- [ ] Menu during "Thinking of a hint..." leaves no stray outline or status
      on the next screen; the computer is not stalled after returning to a game.
- [ ] Hint does not change the score, stats or clocks; the number of hints used is kept for
      Game Over (not shown yet).

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
- [ ] After the last move the final position and the result stay on the board for about 1.5 s
      (clock stopped, Undo disabled, taps ignored), then Game Over (#36) opens with a summary
      that matches the game. Without Game Over the result stays and Menu still works.
- [ ] Vs computer stats count the result once (Stats: played / won / lost / drawn); two-player
      games increment the two-player count.
