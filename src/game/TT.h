#pragma once
// Transposition table (ARCHITECTURE §4, §9). Pure C++17: no heap, no Arduino/ESP-IDF headers.
//
// * Memory is owned by the caller and handed to init(): on the device the AI worker passes
//   heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM) memory (allocated in hal/), on the host a plain
//   heap buffer. The table never allocates or frees.
// * One entry per slot, 12 bytes (static_assert below). The entry count is the largest power of
//   two that fits in the buffer, so the slot is `key & (entries - 1)`; the upper 32 bits of the
//   key are kept in TTEntry::verify to reject most index collisions.
// * Replacement is depth-preferred: a store overwrites the slot if it is empty, holds the same
//   position (same verify), or the new depth is >= the stored depth. When the same position is
//   stored again without a move (pathLen == 0, e.g. a fail-low), the previous move is kept.
// * Scores are stored as the searcher gives them; the searcher converts mate scores to "distance
//   from this node" before storing and back after probing (see Search.cpp).
// * The table belongs to one thread (the AI worker); it is not synchronized.
#include <cstddef>
#include <cstdint>

namespace checkers {

enum class TTFlag : uint8_t { Empty = 0, Exact, Lower, Upper };

struct TTEntry {
  uint32_t verify;  // upper 32 bits of the Zobrist key (set by store())
  int16_t score;    // node score; Lower: true score >= score, Upper: true score <= score
  uint8_t depth;    // remaining depth the score was searched to
  TTFlag flag;      // Empty for an unused slot
  uint8_t from;     // best move, enough to re-identify it among the legal moves:
  uint8_t to;       //   from, to and number of landings; pathLen == 0 means "no move"
  uint8_t pathLen;
  uint8_t pad;  // reserved, always 0
};
static_assert(sizeof(TTEntry) == 12, "TTEntry is 12 bytes");

class TranspositionTable {
 public:
  // Uses `memory` (at least `bytes` long, aligned for TTEntry) for the largest power-of-two
  // number of entries that fits, and clears it. Returns false (table disabled, entries() == 0)
  // if memory is null, misaligned or too small for one entry.
  bool init(void* memory, size_t bytes);
  // Marks every entry empty.
  void clear();
  // Copies the entry for `key` into `out` and returns true if the slot holds that position.
  bool probe(uint64_t key, TTEntry& out) const;
  // Stores `e` for `key` (e.verify is ignored and set from key) per the replacement rule above.
  void store(uint64_t key, const TTEntry& e);

  size_t entries() const { return table_ == nullptr ? 0 : mask_ + 1; }
  static constexpr size_t entryBytes() { return sizeof(TTEntry); }

 private:
  TTEntry* table_ = nullptr;
  size_t mask_ = 0;
};

}  // namespace checkers
