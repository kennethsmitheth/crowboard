#include "game/TT.h"

namespace checkers {

namespace {

uint32_t verifyOf(uint64_t key) {
  return static_cast<uint32_t>(key >> 32);
}

}  // namespace

bool TranspositionTable::init(void* memory, size_t bytes) {
  table_ = nullptr;
  mask_ = 0;
  if (memory == nullptr) return false;
  if (reinterpret_cast<uintptr_t>(memory) % alignof(TTEntry) != 0) return false;
  const size_t fit = bytes / sizeof(TTEntry);
  if (fit == 0) return false;
  size_t n = 1;
  while (n <= fit / 2) n *= 2;
  table_ = static_cast<TTEntry*>(memory);
  mask_ = n - 1;
  clear();
  return true;
}

void TranspositionTable::clear() {
  if (table_ == nullptr) return;
  for (size_t i = 0; i <= mask_; ++i) table_[i] = TTEntry{};
}

bool TranspositionTable::probe(uint64_t key, TTEntry& out) const {
  if (table_ == nullptr) return false;
  const TTEntry& e = table_[key & mask_];
  if (e.flag == TTFlag::Empty || e.verify != verifyOf(key)) return false;
  out = e;
  return true;
}

void TranspositionTable::store(uint64_t key, const TTEntry& e) {
  if (table_ == nullptr) return;
  TTEntry& slot = table_[key & mask_];
  const uint32_t v = verifyOf(key);
  const bool same = slot.flag != TTFlag::Empty && slot.verify == v;
  if (slot.flag != TTFlag::Empty && !same && e.depth < slot.depth) return;
  TTEntry n = e;
  n.verify = v;
  n.pad = 0;
  if (same && n.pathLen == 0) {
    n.from = slot.from;
    n.to = slot.to;
    n.pathLen = slot.pathLen;
  }
  slot = n;
}

}  // namespace checkers
