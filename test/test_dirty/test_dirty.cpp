// Host tests for ui/DirtyRects: cell mask, square numbering, rectangle list policy, iteration.
#include <unity.h>

#include "ui/BoardGeometry.h"
#include "ui/DirtyRects.h"

using namespace checkers::ui;

void setUp(void) {}
void tearDown(void) {}

static void test_empty_by_default(void) {
  DirtyRects d;
  TEST_ASSERT_FALSE(d.any());
  TEST_ASSERT_EQUAL_UINT64(0, d.cells());
  TEST_ASSERT_EQUAL_INT(0, d.cellCount());
  TEST_ASSERT_EQUAL_INT(0, d.rectCount());
}

static void test_mark_cell_sets_row_major_bit(void) {
  DirtyRects d;
  d.markCell(0, 0);
  d.markCell(2, 5);
  d.markCell(7, 7);
  TEST_ASSERT_TRUE(d.any());
  TEST_ASSERT_EQUAL_INT(3, d.cellCount());
  TEST_ASSERT_TRUE(d.cell(0, 0));
  TEST_ASSERT_TRUE(d.cell(2, 5));
  TEST_ASSERT_TRUE(d.cell(7, 7));
  TEST_ASSERT_FALSE(d.cell(5, 2));
  const uint64_t expect = (1ULL << 0) | (1ULL << (2 * 8 + 5)) | (1ULL << 63);
  TEST_ASSERT_EQUAL_UINT64(expect, d.cells());
  TEST_ASSERT_EQUAL_UINT8(0x20, d.rowMask(2));
  TEST_ASSERT_EQUAL_UINT8(0x00, d.rowMask(3));
}

static void test_mark_cell_ignores_out_of_range(void) {
  DirtyRects d;
  d.markCell(-1, 0);
  d.markCell(0, 8);
  d.markCell(8, 0);
  d.markCell(0, -1);
  TEST_ASSERT_FALSE(d.any());
  TEST_ASSERT_FALSE(d.cell(-1, 0));
  TEST_ASSERT_FALSE(d.cell(0, 8));
}

static void test_mark_square_matches_board_geometry(void) {
  for (int sq = 1; sq <= 32; ++sq) {
    DirtyRects d;
    d.markSquare(sq);
    TEST_ASSERT_EQUAL_INT(1, d.cellCount());
    int16_t x, y;
    cellRect(sq, x, y);
    const int row = y / kBoardCell, col = x / kBoardCell;
    TEST_ASSERT_TRUE_MESSAGE(d.cell(row, col), "markSquare must agree with cellRect");
    TEST_ASSERT_EQUAL_INT(1, (row + col) & 1);  // playable squares are dark
  }
  DirtyRects d;
  d.markSquare(0);
  d.markSquare(33);
  TEST_ASSERT_FALSE(d.any());
}

static void test_mark_square_corners(void) {
  DirtyRects d;
  d.markSquare(1);
  TEST_ASSERT_TRUE(d.cell(0, 1));
  d.markSquare(4);
  TEST_ASSERT_TRUE(d.cell(0, 7));
  d.markSquare(5);
  TEST_ASSERT_TRUE(d.cell(1, 0));
  d.markSquare(29);
  TEST_ASSERT_TRUE(d.cell(7, 0));
  d.markSquare(32);
  TEST_ASSERT_TRUE(d.cell(7, 6));
  TEST_ASSERT_EQUAL_INT(5, d.cellCount());
}

static void test_mark_row_and_col(void) {
  DirtyRects d;
  d.markRow(3);
  TEST_ASSERT_EQUAL_INT(8, d.cellCount());
  TEST_ASSERT_EQUAL_UINT8(0xFF, d.rowMask(3));
  d.markCol(6);
  TEST_ASSERT_EQUAL_INT(15, d.cellCount());  // (3,6) counted once
  for (int r = 0; r < 8; ++r) TEST_ASSERT_TRUE(d.cell(r, 6));
  d.markRow(8);
  d.markCol(-1);
  TEST_ASSERT_EQUAL_INT(15, d.cellCount());
}

static void test_mark_cells_under_rect(void) {
  DirtyRects d;
  // A 48 px disc centred on the corner shared by cells (3,3),(3,4),(4,3),(4,4).
  d.markCellsUnder(240 - 24, 240 - 24, 48, 48);
  TEST_ASSERT_EQUAL_INT(4, d.cellCount());
  TEST_ASSERT_TRUE(d.cell(3, 3));
  TEST_ASSERT_TRUE(d.cell(3, 4));
  TEST_ASSERT_TRUE(d.cell(4, 3));
  TEST_ASSERT_TRUE(d.cell(4, 4));
  TEST_ASSERT_EQUAL_INT(0, d.rectCount());  // never touches the rect list

  DirtyRects e;
  e.markCellsUnder(60, 60, 60, 60);  // exactly one cell
  TEST_ASSERT_EQUAL_INT(1, e.cellCount());
  TEST_ASSERT_TRUE(e.cell(1, 1));

  DirtyRects f;
  f.markCellsUnder(470, 470, 100, 100);  // straddles the board edge: clipped to (7,7)
  TEST_ASSERT_EQUAL_INT(1, f.cellCount());
  TEST_ASSERT_TRUE(f.cell(7, 7));
  f.markCellsUnder(500, 10, 50, 50);  // entirely in the sidebar: nothing
  TEST_ASSERT_EQUAL_INT(1, f.cellCount());
}

static void test_mark_rect_basic(void) {
  DirtyRects d;
  d.markRect(500, 20, 100, 40);
  TEST_ASSERT_TRUE(d.any());
  TEST_ASSERT_EQUAL_INT(1, d.rectCount());
  TEST_ASSERT_TRUE(Rect({500, 20, 100, 40}) == d.rect(0));
  TEST_ASSERT_EQUAL_INT(0, d.cellCount());
}

static void test_mark_rect_clips_and_drops_empty(void) {
  DirtyRects d;
  d.markRect(-10, -10, 30, 30);
  TEST_ASSERT_EQUAL_INT(1, d.rectCount());
  TEST_ASSERT_TRUE(Rect({0, 0, 20, 20}) == d.rect(0));
  d.markRect(790, 470, 100, 100);
  TEST_ASSERT_EQUAL_INT(2, d.rectCount());
  TEST_ASSERT_TRUE(Rect({790, 470, 10, 10}) == d.rect(1));
  d.markRect(100, 100, 0, 10);
  d.markRect(100, 100, 10, -5);
  d.markRect(900, 100, 10, 10);  // entirely off screen
  TEST_ASSERT_EQUAL_INT(2, d.rectCount());
}

static void test_mark_rect_containment(void) {
  DirtyRects d;
  d.markRect(100, 100, 50, 50);
  d.markRect(110, 110, 10, 10);  // inside: ignored
  TEST_ASSERT_EQUAL_INT(1, d.rectCount());
  d.markRect(100, 100, 50, 50);  // identical: ignored
  TEST_ASSERT_EQUAL_INT(1, d.rectCount());
  d.markRect(200, 200, 10, 10);
  d.markRect(300, 300, 10, 10);
  TEST_ASSERT_EQUAL_INT(3, d.rectCount());
  d.markRect(90, 90, 300, 300);  // covers all three: replaces them
  TEST_ASSERT_EQUAL_INT(1, d.rectCount());
  TEST_ASSERT_TRUE(Rect({90, 90, 300, 300}) == d.rect(0));
}

static void test_mark_rect_merges_when_full(void) {
  DirtyRects d;
  for (int i = 0; i < DirtyRects::kMaxRects; ++i)
    d.markRect(static_cast<int16_t>(i * 90), 0, 10, 10);
  TEST_ASSERT_EQUAL_INT(DirtyRects::kMaxRects, d.rectCount());
  // Closest to the rect at x = 270 (the 4th one): union with it grows least.
  d.markRect(285, 0, 10, 10);
  TEST_ASSERT_EQUAL_INT(DirtyRects::kMaxRects, d.rectCount());
  bool merged = false;
  for (int i = 0; i < d.rectCount(); ++i) {
    if (d.rect(i) == Rect{270, 0, 25, 10}) merged = true;
  }
  TEST_ASSERT_TRUE_MESSAGE(merged, "new rect should merge into its nearest neighbour");
  // Every marked region is still covered by some listed rectangle.
  const Rect probes[] = {{0, 0, 10, 10}, {630, 0, 10, 10}, {285, 0, 10, 10}, {270, 0, 10, 10}};
  for (const Rect& p : probes) {
    bool covered = false;
    for (int i = 0; i < d.rectCount(); ++i) covered = covered || d.rect(i).contains(p);
    TEST_ASSERT_TRUE_MESSAGE(covered, "a full list must never lose a region");
  }
}

static void test_mark_all_and_clear(void) {
  DirtyRects d;
  d.markAll();
  TEST_ASSERT_TRUE(d.any());
  TEST_ASSERT_EQUAL_INT(64, d.cellCount());
  TEST_ASSERT_EQUAL_UINT64(~0ULL, d.cells());
  TEST_ASSERT_EQUAL_INT(1, d.rectCount());
  TEST_ASSERT_TRUE(DirtyRects::kScreen == d.rect(0));
  TEST_ASSERT_EQUAL_INT16(kScreenW, d.rect(0).w);
  TEST_ASSERT_EQUAL_INT16(kScreenH, d.rect(0).h);
  d.clear();
  TEST_ASSERT_FALSE(d.any());
  TEST_ASSERT_EQUAL_INT(0, d.cellCount());
  TEST_ASSERT_EQUAL_INT(0, d.rectCount());
}

static void test_for_each_cell_row_major(void) {
  DirtyRects d;
  d.markCell(5, 2);
  d.markCell(0, 7);
  d.markCell(5, 0);
  d.markCell(7, 7);
  int rows[8], cols[8], n = 0;
  d.forEachCell([&](int r, int c) {
    rows[n] = r;
    cols[n] = c;
    ++n;
  });
  TEST_ASSERT_EQUAL_INT(4, n);
  TEST_ASSERT_EQUAL_INT(0, rows[0]);
  TEST_ASSERT_EQUAL_INT(7, cols[0]);
  TEST_ASSERT_EQUAL_INT(5, rows[1]);
  TEST_ASSERT_EQUAL_INT(0, cols[1]);
  TEST_ASSERT_EQUAL_INT(5, rows[2]);
  TEST_ASSERT_EQUAL_INT(2, cols[2]);
  TEST_ASSERT_EQUAL_INT(7, rows[3]);
  TEST_ASSERT_EQUAL_INT(7, cols[3]);
}

static void test_for_each_rect_insertion_order(void) {
  DirtyRects d;
  d.markRect(10, 10, 5, 5);
  d.markRect(20, 20, 5, 5);
  d.markRect(30, 30, 5, 5);
  int16_t xs[3], n = 0;
  d.forEachRect([&](const Rect& r) { xs[n++] = r.x; });
  TEST_ASSERT_EQUAL_INT(3, n);
  TEST_ASSERT_EQUAL_INT16(10, xs[0]);
  TEST_ASSERT_EQUAL_INT16(20, xs[1]);
  TEST_ASSERT_EQUAL_INT16(30, xs[2]);
}

static void test_cell_pixel_helpers(void) {
  TEST_ASSERT_EQUAL_INT16(0, DirtyRects::cellX(0));
  TEST_ASSERT_EQUAL_INT16(420, DirtyRects::cellX(7));
  TEST_ASSERT_EQUAL_INT16(180, DirtyRects::cellY(3));
  int16_t x, y;
  cellRect(14, x, y);  // row 3, col 2
  TEST_ASSERT_EQUAL_INT16(x, DirtyRects::cellX(2));
  TEST_ASSERT_EQUAL_INT16(y, DirtyRects::cellY(3));
}

static void test_rect_helpers(void) {
  const Rect a{0, 0, 10, 10}, b{5, 5, 10, 10}, c{20, 20, 1, 1}, e{};
  TEST_ASSERT_TRUE(a.intersects(b));
  TEST_ASSERT_FALSE(a.intersects(c));
  TEST_ASSERT_FALSE(a.intersects(e));
  TEST_ASSERT_TRUE(a.unite(b) == Rect({0, 0, 15, 15}));
  TEST_ASSERT_TRUE(a.unite(e) == a);
  TEST_ASSERT_TRUE(e.unite(c) == c);
  TEST_ASSERT_EQUAL_INT32(100, a.area());
  TEST_ASSERT_EQUAL_INT32(0, e.area());
  TEST_ASSERT_TRUE(Rect({0, 0, 20, 20}).contains(a));
  TEST_ASSERT_FALSE(a.contains(b));
  TEST_ASSERT_FALSE(a.contains(e));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_empty_by_default);
  RUN_TEST(test_mark_cell_sets_row_major_bit);
  RUN_TEST(test_mark_cell_ignores_out_of_range);
  RUN_TEST(test_mark_square_matches_board_geometry);
  RUN_TEST(test_mark_square_corners);
  RUN_TEST(test_mark_row_and_col);
  RUN_TEST(test_mark_cells_under_rect);
  RUN_TEST(test_mark_rect_basic);
  RUN_TEST(test_mark_rect_clips_and_drops_empty);
  RUN_TEST(test_mark_rect_containment);
  RUN_TEST(test_mark_rect_merges_when_full);
  RUN_TEST(test_mark_all_and_clear);
  RUN_TEST(test_for_each_cell_row_major);
  RUN_TEST(test_for_each_rect_insertion_order);
  RUN_TEST(test_cell_pixel_helpers);
  RUN_TEST(test_rect_helpers);
  return UNITY_END();
}
