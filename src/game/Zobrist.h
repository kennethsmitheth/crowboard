#pragma once
// Zobrist hashing (ARCHITECTURE §4). Pure C++17, no heap.
//
// * One 64-bit key per (piece kind, square) for the four kinds BlackMan, BlackKing, WhiteMan,
//   WhiteKing and the 32 squares, plus one key XORed in when White is to move.
// * Keys come from splitmix64 seeded with kZobristSeed and are generated at compile time, so the
//   hash of a position is the same on every run and every platform (the host tests lock the
//   value for the initial position).
// * zobristHash() computes a key from scratch; zobristApply() updates one incrementally for a
//   single move and always equals zobristHash(applyMove(before, m)).
#include <cstdint>

#include "game/Board.h"
#include "game/Move.h"

namespace checkers {

constexpr uint64_t kZobristSeed = 0x43524F57424F4152ULL;  // "CROWBOAR"
constexpr int kZobristKinds = 4;                          // index = static_cast<int>(Piece) - 1

namespace detail {

constexpr uint64_t splitmix64(uint64_t& state) {
  uint64_t z = (state += 0x9E3779B97F4A7C15ULL);
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
  return z ^ (z >> 31);
}

struct ZobristKeys {
  uint64_t piece[kZobristKinds][kNumSquares];
  uint64_t whiteToMove;
};

constexpr ZobristKeys makeZobristKeys() {
  ZobristKeys k{};
  uint64_t state = kZobristSeed;
  for (int p = 0; p < kZobristKinds; ++p) {
    for (int sq = 0; sq < kNumSquares; ++sq) k.piece[p][sq] = splitmix64(state);
  }
  k.whiteToMove = splitmix64(state);
  return k;
}

inline constexpr ZobristKeys kZobristKeys = makeZobristKeys();

}  // namespace detail

// Key for piece p (not Piece::None) on square sq.
constexpr uint64_t zobristKey(Piece p, Square sq) {
  return detail::kZobristKeys.piece[static_cast<int>(p) - 1][sq];
}
// XORed into the hash when White is to move.
constexpr uint64_t zobristSideKey() {
  return detail::kZobristKeys.whiteToMove;
}

// Hash of b computed from scratch.
uint64_t zobristHash(const Board& b);

// Hash of applyMove(before, m), given h == zobristHash(before). Same precondition as apply():
// m is a legal move for `before`.
uint64_t zobristApply(uint64_t h, const Board& before, const Move& m);

}  // namespace checkers
