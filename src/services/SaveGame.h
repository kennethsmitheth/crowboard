#pragma once
// Autosave of an in-progress game through IStorage as one versioned blob:
//   namespace "save", key "v1", blob magic 'SAV1', blob version 1.
// Payload (little-endian, no padding):
//   mode(1) difficulty(1) humanSide(1) forcedCapture(1) p1ClockSec(4) p2ClockSec(4)
//   moveCount(2) then moveCount x { from(1) pathLen(1) path[pathLen] } with 0-based squares.
// load() rebuilds the game by replaying every move from the initial position, which also
// validates the save: any illegal move makes load() fail. Pure: no Arduino or ESP-IDF includes.
#include <stddef.h>
#include <stdint.h>

#include "GameTypes.h"
#include "IStorage.h"
#include "game/Game.h"
#include "services/Blob.h"

namespace checkers {

struct SaveHeader {
  uint8_t mode;           // 0 vs computer, 1 two players
  uint8_t difficulty;     // Difficulty value (vs computer)
  uint8_t humanSide;      // 0 Black (P1), 1 White (P2)
  uint8_t forcedCapture;  // 0 or 1; save() takes it from game.rules()
  uint32_t p1ClockSec;
  uint32_t p2ClockSec;
  uint16_t moveCount;  // filled by load(); ignored by save() (taken from the game)
};

class SaveGame {
 public:
  static constexpr const char* kNamespace = "save";
  static constexpr const char* kKey = "v1";
  static constexpr uint32_t kMagic = 0x53415631u;  // "SAV1"
  static constexpr uint16_t kVersion = 1;

  static constexpr size_t kHeaderBytes = 14;
  static constexpr size_t kMoveBytesCapacity = Blob::kMaxPayload - kHeaderBytes;
  // Simple moves take 3 bytes, so kMaxPlies simple moves always fit. A very long game full of
  // multi-jumps could exceed the byte budget; save() then returns false and writes nothing.
  static constexpr size_t kMaxSavedMoves = kMoveBytesCapacity / 3;
  static_assert(kMaxSavedMoves >= kMaxPlies, "a full game of simple moves must fit one blob");

  explicit SaveGame(IStorage& storage) : storage_(storage) {}

  // True if a valid (magic, version, CRC) blob is present. Does not replay the moves.
  bool exists();
  // Encodes the game's move history and header. False if it does not fit or storage fails.
  bool save(const Game& game, const SaveHeader& header);
  // Replays the save into outGame. On any failure outGame is left reset and false returned.
  bool load(Game& outGame, SaveHeader& outHeader);
  // Removes the save.
  void erase();

 private:
  IStorage& storage_;
};

}  // namespace checkers
