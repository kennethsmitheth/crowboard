// Zobrist hashing and the transposition table (ARCHITECTURE §4, issue #16).
#include <unity.h>

#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "game/Apply.h"
#include "game/Game.h"
#include "game/MoveGen.h"
#include "game/Search.h"
#include "game/TT.h"
#include "game/Zobrist.h"

using namespace checkers;

void setUp(void) {}
void tearDown(void) {}

// Large objects outside any stack frame (as on the device).
static Searcher g_searcher;
static TranspositionTable g_tt;
// 2 MB, a plausible PSRAM budget: 131072 entries of 12 bytes. uint64_t keeps it aligned.
static std::vector<uint64_t> g_ttMemory(2u * 1024u * 1024u / sizeof(uint64_t));

static Board parse(const char* s) {
  Board b = Board::emptyBoard();
  TEST_ASSERT_TRUE_MESSAGE(Board::fromString(s, b), s);
  return b;
}

static std::string str(const Board& b) {
  char buf[kPositionStringMax];
  b.toString(buf, sizeof buf);
  return buf;
}

struct Lcg {
  uint64_t s;
  uint32_t next() {
    s = s * 6364136223846793005ULL + 1442695040888963407ULL;
    return static_cast<uint32_t>(s >> 33);
  }
};

// ---- Zobrist ---------------------------------------------------------------------------------

static void test_initial_hash_is_fixed(void) {
  // Locked: changing the seed or key layout changes every stored hash.
  TEST_ASSERT_EQUAL_HEX64(0xAC0CAC203870E38BULL, zobristHash(Board::initial()));
  TEST_ASSERT_EQUAL_HEX64(0, zobristHash(Board::emptyBoard()));
  TEST_ASSERT_EQUAL_HEX64(zobristHash(Board::initial()), hashBoard(Board::initial()));
}

static void test_side_to_move_matters(void) {
  Board b = Board::initial();
  const uint64_t black = zobristHash(b);
  b.side = Side::White;
  const uint64_t white = zobristHash(b);
  TEST_ASSERT_TRUE(black != white);
  TEST_ASSERT_EQUAL_HEX64(zobristSideKey(), black ^ white);
  // Kind matters too: a man and a king on the same square hash differently.
  TEST_ASSERT_TRUE(zobristHash(parse("B:W1:B32")) != zobristHash(parse("B:W1:BK32")));
  TEST_ASSERT_TRUE(zobristHash(parse("B:W1:B32")) != zobristHash(parse("B:WK1:B32")));
}

struct Coverage {
  uint32_t moves = 0, captures = 0, multiJumps = 0, promotions = 0, kingCaptures = 0,
           capturedKings = 0, loops = 0;
};

// Every legal move of b: incremental update equals recomputation.
static void checkAllMoves(const Board& b, bool forcedCapture, Coverage& c) {
  const uint64_t h = zobristHash(b);
  MoveList list;
  legalMoves(b, list, forcedCapture);
  for (const Move& m : list) {
    const Board after = applyMove(b, m);
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(zobristHash(after), zobristApply(h, b, m), str(b).c_str());
    ++c.moves;
    if (m.isCapture()) ++c.captures;
    if (m.captureCount() > 1) ++c.multiJumps;
    if (m.promotes) ++c.promotions;
    if (m.isCapture() && (b.kings[sideIndex(b.side)] & bit(m.from))) ++c.kingCaptures;
    if (m.captured & b.kings[sideIndex(opposite(b.side))]) ++c.capturedKings;
    if (m.from == m.to) ++c.loops;
  }
}

static void test_incremental_matches_handpicked(void) {
  const char* const positions[] = {
      "B:W1,2,3,4,5,6,7,8,9,10,11,12:B21,22,23,24,25,26,27,28,29,30,31,32",
      "W:W18,24,27,28,K10,K15:B12,16,20,K22",
      "B:W18,24,27,28,K10,K15:B12,16,20,K22",
      "B:W6,7,K14,K22:B15,19,K26",
      "B:W2,3,10,11:BK18,19",    // king multi-jumps
      "W:W21,22:B26,K27,29,30",  // White promotes by capture / step
      "B:WK6,K7,K14,K15:BK10",   // king surrounded by kings: loop captures possible
      "B:W5,6,7,13,14,15:B18,22",
      "B:W14,15,22,23:BK10",  // the king captures all four and lands back on 10 (from == to)
  };
  Coverage c;
  for (const char* s : positions) {
    const Board b = parse(s);
    checkAllMoves(b, true, c);
    checkAllMoves(b, false, c);
  }
  TEST_ASSERT_TRUE(c.moves > 50);
  TEST_ASSERT_TRUE(c.captures > 0);
  TEST_ASSERT_TRUE(c.multiJumps > 0);
  TEST_ASSERT_TRUE(c.loops > 0);
}

static void test_incremental_matches_random_games(void) {
  static Game g;
  Lcg rng{0x2B0B57ULL};
  Coverage c;
  uint32_t sequences = 0;
  for (int game = 0; game < 150; ++game) {
    g.reset();
    uint64_t h = zobristHash(g.board());
    while (!g.isOver() && g.ply() < kMaxPlies) {
      const Board b = g.board();
      checkAllMoves(b, true, c);
      checkAllMoves(b, false, c);
      const MoveList& l = g.legalMoves();
      const Move m = l[rng.next() % l.size()];
      // Sequence: the running incremental hash never drifts from the recomputed one.
      h = zobristApply(h, b, m);
      TEST_ASSERT_TRUE(g.apply(m));
      TEST_ASSERT_EQUAL_HEX64(zobristHash(g.board()), h);
      TEST_ASSERT_EQUAL_HEX64(h, hashBoard(g.board()));
      ++sequences;
    }
  }
  char msg[200];
  snprintf(msg, sizeof msg,
           "zobrist: %u moves checked (%u captures, %u multi-jumps, %u promotions, %u king "
           "captures, %u kings captured, %u loops), %u sequence steps",
           c.moves, c.captures, c.multiJumps, c.promotions, c.kingCaptures, c.capturedKings,
           c.loops, sequences);
  TEST_MESSAGE(msg);
  TEST_ASSERT_TRUE(c.multiJumps > 0);
  TEST_ASSERT_TRUE(c.promotions > 0);
  TEST_ASSERT_TRUE(c.kingCaptures > 0);
  TEST_ASSERT_TRUE(c.capturedKings > 0);
}

static void test_distinct_positions_distinct_hashes(void) {
  static Game g;
  Lcg rng{0xD15717C7ULL};
  std::map<uint64_t, std::string> seen;
  for (int game = 0; game < 300; ++game) {
    g.reset();
    while (!g.isOver() && g.ply() < kMaxPlies) {
      const std::string s = str(g.board());
      const uint64_t h = zobristHash(g.board());
      auto it = seen.find(h);
      if (it == seen.end()) {
        seen.emplace(h, s);
      } else {
        TEST_ASSERT_EQUAL_STRING(it->second.c_str(), s.c_str());  // same hash => same position
      }
      const MoveList& l = g.legalMoves();
      TEST_ASSERT_TRUE(g.apply(l[rng.next() % l.size()]));
    }
  }
  char msg[64];
  snprintf(msg, sizeof msg, "zobrist: %u distinct positions, no collision",
           static_cast<unsigned>(seen.size()));
  TEST_MESSAGE(msg);
  TEST_ASSERT_TRUE(seen.size() > 10000);
}

// ---- Transposition table ---------------------------------------------------------------------

static TTEntry entry(int score, int depth, TTFlag flag, int from = 0, int to = 0, int len = 0) {
  TTEntry e{};
  e.score = static_cast<int16_t>(score);
  e.depth = static_cast<uint8_t>(depth);
  e.flag = flag;
  e.from = static_cast<uint8_t>(from);
  e.to = static_cast<uint8_t>(to);
  e.pathLen = static_cast<uint8_t>(len);
  return e;
}

static void test_tt_init(void) {
  TranspositionTable t;
  TEST_ASSERT_EQUAL_UINT(0, t.entries());
  TEST_ASSERT_FALSE(t.init(nullptr, 1024));
  alignas(8) static uint8_t small[64];
  TEST_ASSERT_FALSE(t.init(small, 11));
  TEST_ASSERT_EQUAL_UINT(0, t.entries());
  TEST_ASSERT_FALSE(t.init(small + 1, 32));  // misaligned
  TEST_ASSERT_TRUE(t.init(small, 12));
  TEST_ASSERT_EQUAL_UINT(1, t.entries());
  TEST_ASSERT_TRUE(t.init(small, 64));  // 5 fit, power of two below is 4
  TEST_ASSERT_EQUAL_UINT(4, t.entries());
  TEST_ASSERT_EQUAL_UINT(12, TranspositionTable::entryBytes());
  // Disabled table: probe misses, store is a no-op.
  TranspositionTable off;
  off.store(1, entry(5, 3, TTFlag::Exact));
  TTEntry out;
  TEST_ASSERT_FALSE(off.probe(1, out));

  TEST_ASSERT_TRUE(g_tt.init(g_ttMemory.data(), g_ttMemory.size() * sizeof(uint64_t)));
  TEST_ASSERT_EQUAL_UINT(131072, g_tt.entries());
}

static void test_tt_round_trip_and_verify(void) {
  TEST_ASSERT_TRUE(g_tt.init(g_ttMemory.data(), g_ttMemory.size() * sizeof(uint64_t)));
  const uint64_t key = 0x123456789ABCDEF0ULL;
  TTEntry out;
  TEST_ASSERT_FALSE(g_tt.probe(key, out));
  g_tt.store(key, entry(-1234, 7, TTFlag::Lower, 22, 15, 1));
  TEST_ASSERT_TRUE(g_tt.probe(key, out));
  TEST_ASSERT_EQUAL_INT(-1234, out.score);
  TEST_ASSERT_EQUAL_UINT8(7, out.depth);
  TEST_ASSERT_TRUE(out.flag == TTFlag::Lower);
  TEST_ASSERT_EQUAL_UINT8(22, out.from);
  TEST_ASSERT_EQUAL_UINT8(15, out.to);
  TEST_ASSERT_EQUAL_UINT8(1, out.pathLen);
  TEST_ASSERT_EQUAL_HEX32(0x12345678u, out.verify);
  // Same slot (same low bits), different upper bits: rejected.
  TEST_ASSERT_FALSE(g_tt.probe(key ^ (uint64_t{1} << 40), out));
  // Different slot: miss.
  TEST_ASSERT_FALSE(g_tt.probe(key + 1, out));
  // A key of 0 never matches an empty slot.
  TEST_ASSERT_FALSE(g_tt.probe(0, out));
  g_tt.store(0, entry(1, 1, TTFlag::Exact));
  TEST_ASSERT_TRUE(g_tt.probe(0, out));
  TEST_ASSERT_EQUAL_INT(1, out.score);
}

static void test_tt_replacement(void) {
  TEST_ASSERT_TRUE(g_tt.init(g_ttMemory.data(), g_ttMemory.size() * sizeof(uint64_t)));
  const uint64_t a = 0xAAAA000000000005ULL;
  const uint64_t b = 0xBBBB000000000005ULL;  // same slot as a
  TTEntry out;
  g_tt.store(a, entry(10, 6, TTFlag::Exact, 1, 5, 1));
  // Shallower other position: kept out.
  g_tt.store(b, entry(20, 5, TTFlag::Exact));
  TEST_ASSERT_TRUE(g_tt.probe(a, out));
  TEST_ASSERT_FALSE(g_tt.probe(b, out));
  // Same position: always replaced, even by a shallower result; a move-less store keeps the move.
  g_tt.store(a, entry(11, 2, TTFlag::Upper));
  TEST_ASSERT_TRUE(g_tt.probe(a, out));
  TEST_ASSERT_EQUAL_INT(11, out.score);
  TEST_ASSERT_EQUAL_UINT8(2, out.depth);
  TEST_ASSERT_TRUE(out.flag == TTFlag::Upper);
  TEST_ASSERT_EQUAL_UINT8(1, out.from);
  TEST_ASSERT_EQUAL_UINT8(5, out.to);
  TEST_ASSERT_EQUAL_UINT8(1, out.pathLen);
  // Other position with equal depth: replaces.
  g_tt.store(b, entry(20, 2, TTFlag::Lower, 9, 2, 3));
  TEST_ASSERT_FALSE(g_tt.probe(a, out));
  TEST_ASSERT_TRUE(g_tt.probe(b, out));
  TEST_ASSERT_EQUAL_INT(20, out.score);
  TEST_ASSERT_EQUAL_UINT8(3, out.pathLen);
  // Deeper other position: replaces, and its (absent) move is not inherited from b.
  g_tt.store(a, entry(30, 9, TTFlag::Exact));
  TEST_ASSERT_TRUE(g_tt.probe(a, out));
  TEST_ASSERT_EQUAL_UINT8(0, out.pathLen);
  TEST_ASSERT_FALSE(g_tt.probe(b, out));

  // clear(): everything empty.
  g_tt.clear();
  TEST_ASSERT_FALSE(g_tt.probe(a, out));
  // After clear an empty slot is always filled, whatever the depth.
  g_tt.store(b, entry(1, 0, TTFlag::Exact));
  TEST_ASSERT_TRUE(g_tt.probe(b, out));
}

// ---- Search integration ----------------------------------------------------------------------

static SearchResult run(const Board& b, int depth, bool withTT) {
  SearchLimits limits;
  limits.maxDepth = depth;
  g_tt.clear();
  g_searcher.setTT(withTT ? &g_tt : nullptr);
  const SearchResult r = g_searcher.search(b, limits, EvalWeights{});
  g_searcher.setTT(nullptr);
  return r;
}

static void test_search_same_result_with_tt(void) {
  TEST_ASSERT_TRUE(g_tt.init(g_ttMemory.data(), g_ttMemory.size() * sizeof(uint64_t)));
  const char* const positions[] = {
      "B:W1,2,3,4,5,6,7,8,9,10,11,12:B21,22,23,24,25,26,27,28,29,30,31,32",
      "W:W18,24,27,28,K10,K15:B12,16,20,K22",
      "B:W18,24,27,28,K10,K15:B12,16,20,K22",
      "B:W9,10,17,19:B16,27,30,31",
      "W:W5,9,K14:B21,K26,30",
      "B:W1,2,3,4,6,7,8,9,10,12:B21,22,23,24,25,26,27,29,30,31",
      "B:WK29:B22,K30",  // mate in 1 (mate-score storage across plies)
      "W:WK29:B22,K30",  // slow loss
  };
  for (const char* s : positions) {
    const Board b = parse(s);
    for (int depth = 1; depth <= 6; ++depth) {
      const SearchResult plain = run(b, depth, false);
      const SearchResult hashed = run(b, depth, true);
      char msg[160];
      snprintf(msg, sizeof msg, "%s depth %d: score %d vs %d, nodes %u vs %u", s, depth,
               plain.score, hashed.score, static_cast<unsigned>(plain.nodes),
               static_cast<unsigned>(hashed.nodes));
      TEST_ASSERT_TRUE_MESSAGE(hashed.completed, msg);
      TEST_ASSERT_EQUAL_INT_MESSAGE(plain.depth, hashed.depth, msg);
      TEST_ASSERT_EQUAL_INT_MESSAGE(plain.score, hashed.score, msg);
      TEST_ASSERT_TRUE_MESSAGE(plain.best == hashed.best, msg);
    }
  }
}

static void test_search_tt_cuts_nodes_at_depth_8(void) {
  TEST_ASSERT_TRUE(g_tt.init(g_ttMemory.data(), g_ttMemory.size() * sizeof(uint64_t)));
  const Board b = Board::initial();
  const SearchResult plain = run(b, 8, false);
  const SearchResult hashed = run(b, 8, true);
  const double reduction = 100.0 * (1.0 - static_cast<double>(hashed.nodes) / plain.nodes);
  char msg[160];
  snprintf(msg, sizeof msg,
           "initial depth 8: %u nodes without TT, %u with (%.1f%% fewer); score %d vs %d",
           static_cast<unsigned>(plain.nodes), static_cast<unsigned>(hashed.nodes), reduction,
           plain.score, hashed.score);
  printf("%s\n", msg);
  TEST_MESSAGE(msg);
  TEST_ASSERT_TRUE(plain.completed && hashed.completed);
  TEST_ASSERT_TRUE_MESSAGE(plain.best == hashed.best, msg);
  TEST_ASSERT_EQUAL_INT_MESSAGE(plain.score, hashed.score, msg);
  TEST_ASSERT_TRUE_MESSAGE(reduction >= 30.0, msg);
}

static void test_search_tt_kept_between_searches(void) {
  // A second search over a warm table still returns a legal move with the same score.
  TEST_ASSERT_TRUE(g_tt.init(g_ttMemory.data(), g_ttMemory.size() * sizeof(uint64_t)));
  const Board b = parse("W:W18,24,27,28,K10,K15:B12,16,20,K22");
  const SearchResult cold = run(b, 6, true);
  g_searcher.setTT(&g_tt);
  SearchLimits limits;
  limits.maxDepth = 6;
  const SearchResult warm = g_searcher.search(b, limits, EvalWeights{});
  g_searcher.setTT(nullptr);
  TEST_ASSERT_TRUE(warm.completed);
  TEST_ASSERT_TRUE(warm.hasMove());
  TEST_ASSERT_TRUE(cold.best == warm.best);
  TEST_ASSERT_TRUE(warm.nodes < cold.nodes);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_initial_hash_is_fixed);
  RUN_TEST(test_side_to_move_matters);
  RUN_TEST(test_incremental_matches_handpicked);
  RUN_TEST(test_incremental_matches_random_games);
  RUN_TEST(test_distinct_positions_distinct_hashes);
  RUN_TEST(test_tt_init);
  RUN_TEST(test_tt_round_trip_and_verify);
  RUN_TEST(test_tt_replacement);
  RUN_TEST(test_search_same_result_with_tt);
  RUN_TEST(test_search_tt_cuts_nodes_at_depth_8);
  RUN_TEST(test_search_tt_kept_between_searches);
  return UNITY_END();
}
