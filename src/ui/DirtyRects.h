// Dirty-region bookkeeping for the incremental renderer (docs/ARCHITECTURE.md section 7).
//
// Two independent lists:
//   - a 64-bit mask of the 8x8 board cells (bit = row * 8 + col, row 0 at the top), and
//   - up to kMaxRects arbitrary screen rectangles for HUD regions.
// Marking a rectangle never marks cells and vice versa; the Renderer paints cells through its
// CellPainter and rectangles through its RectPainter.
//
// Rectangle policy: a new rectangle is clipped to the screen and dropped if empty. One that is
// already covered by a listed rectangle is ignored; one that covers listed rectangles replaces
// them. When the list is full the new rectangle is merged into the listed rectangle whose union
// grows the least, so a mark is never lost: the cost of a full list is overdraw, not a stale
// region.
//
// Plain C++17 with no Arduino includes so it compiles in the native test environment.
#pragma once

#include <stdint.h>

#include "Theme.h"

namespace checkers::ui {

struct Rect {
  int16_t x = 0;
  int16_t y = 0;
  int16_t w = 0;
  int16_t h = 0;

  constexpr bool empty() const { return w <= 0 || h <= 0; }
  constexpr int32_t area() const { return empty() ? 0 : static_cast<int32_t>(w) * h; }
  constexpr int16_t right() const { return static_cast<int16_t>(x + w); }   // exclusive
  constexpr int16_t bottom() const { return static_cast<int16_t>(y + h); }  // exclusive

  constexpr bool contains(const Rect& o) const {
    return !o.empty() && o.x >= x && o.y >= y && o.right() <= right() && o.bottom() <= bottom();
  }
  constexpr bool intersects(const Rect& o) const {
    return !empty() && !o.empty() && o.x < right() && x < o.right() && o.y < bottom() &&
           y < o.bottom();
  }
  // Smallest rectangle covering both. An empty operand yields the other one.
  constexpr Rect unite(const Rect& o) const {
    if (empty()) return o;
    if (o.empty()) return *this;
    const int16_t nx = x < o.x ? x : o.x;
    const int16_t ny = y < o.y ? y : o.y;
    const int16_t nr = right() > o.right() ? right() : o.right();
    const int16_t nb = bottom() > o.bottom() ? bottom() : o.bottom();
    return Rect{nx, ny, static_cast<int16_t>(nr - nx), static_cast<int16_t>(nb - ny)};
  }
  constexpr bool operator==(const Rect& o) const {
    return x == o.x && y == o.y && w == o.w && h == o.h;
  }
  constexpr bool operator!=(const Rect& o) const { return !(*this == o); }
};

class DirtyRects {
 public:
  static constexpr int kMaxRects = 8;
  static constexpr Rect kScreen{0, 0, kScreenW, kScreenH};

  // ---- marking ------------------------------------------------------------------------------

  // Marks one board cell; out-of-range coordinates are ignored.
  void markCell(int row, int col) {
    if (row < 0 || row >= kBoardCells || col < 0 || col >= kBoardCells) return;
    cells_ |= cellBit(row, col);
  }

  // Marks a playable square 1..32 (ARCHITECTURE section 6: row = (sq-1)/4, squares 1-4 on
  // columns 1,3,5,7 of row 0). Out-of-range squares are ignored.
  void markSquare(int square) {
    if (square < 1 || square > 32) return;
    const int row = (square - 1) / 4;
    const int k = (square - 1) % 4;
    const int col = 2 * k + ((row & 1) ? 0 : 1);
    cells_ |= cellBit(row, col);
  }

  // Marks every cell in the given row (0..7) or column (0..7).
  void markRow(int row) {
    if (row < 0 || row >= kBoardCells) return;
    cells_ |= static_cast<uint64_t>(0xFF) << (row * 8);
  }
  void markCol(int col) {
    if (col < 0 || col >= kBoardCells) return;
    cells_ |= static_cast<uint64_t>(0x0101010101010101ULL) << col;
  }

  // Marks every cell whose 60x60 area intersects the given screen rectangle (board cells only;
  // the rectangle is not added to the rect list). Handy for a sprite's bounding box.
  void markCellsUnder(int16_t x, int16_t y, int16_t w, int16_t h) {
    const Rect r = clip(Rect{x, y, w, h}, Rect{kBoardX, kBoardY, kBoardSize, kBoardSize});
    if (r.empty()) return;
    const int c0 = (r.x - kBoardX) / kCell, c1 = (r.right() - 1 - kBoardX) / kCell;
    const int r0 = (r.y - kBoardY) / kCell, r1 = (r.bottom() - 1 - kBoardY) / kCell;
    for (int row = r0; row <= r1; ++row)
      for (int col = c0; col <= c1; ++col) cells_ |= cellBit(row, col);
  }

  // Adds a screen rectangle; see the policy in the file comment.
  void markRect(int16_t x, int16_t y, int16_t w, int16_t h) { markRect(Rect{x, y, w, h}); }
  void markRect(Rect r) {
    r = clip(r, kScreen);
    if (r.empty()) return;
    int i = 0;
    while (i < count_) {
      if (rects_[i].contains(r)) return;
      if (r.contains(rects_[i])) {
        rects_[i] = rects_[--count_];  // swallow the smaller one; order is not significant
        continue;
      }
      ++i;
    }
    if (count_ < kMaxRects) {
      rects_[count_++] = r;
      return;
    }
    int best = 0;
    int32_t bestGrowth = INT32_MAX;
    for (int j = 0; j < count_; ++j) {
      const int32_t growth = rects_[j].unite(r).area() - rects_[j].area();
      if (growth < bestGrowth) {
        bestGrowth = growth;
        best = j;
      }
    }
    rects_[best] = rects_[best].unite(r);
  }

  // Everything: all 64 cells plus one full-screen rectangle. Rect painters must therefore clip
  // to their own regions; the board area is repainted by the cell painter.
  void markAll() {
    cells_ = ~static_cast<uint64_t>(0);
    count_ = 1;
    rects_[0] = kScreen;
  }

  void clear() {
    cells_ = 0;
    count_ = 0;
  }

  // ---- queries ------------------------------------------------------------------------------

  bool any() const { return cells_ != 0 || count_ != 0; }
  uint64_t cells() const { return cells_; }
  bool cell(int row, int col) const {
    if (row < 0 || row >= kBoardCells || col < 0 || col >= kBoardCells) return false;
    return (cells_ & cellBit(row, col)) != 0;
  }
  int cellCount() const { return __builtin_popcountll(cells_); }
  // Cells marked in one row, as an 8-bit mask (bit = col).
  uint8_t rowMask(int row) const {
    if (row < 0 || row >= kBoardCells) return 0;
    return static_cast<uint8_t>(cells_ >> (row * 8));
  }
  int rectCount() const { return count_; }
  const Rect& rect(int i) const { return rects_[i]; }

  // Calls f(row, col) for each marked cell in row-major order.
  template <class F>
  void forEachCell(F&& f) const {
    uint64_t m = cells_;
    while (m) {
      const int i = __builtin_ctzll(m);
      f(i / 8, i % 8);
      m &= m - 1;
    }
  }

  // Calls f(const Rect&) for each listed rectangle, in insertion order.
  template <class F>
  void forEachRect(F&& f) const {
    for (int i = 0; i < count_; ++i) f(rects_[i]);
  }

  // Top-left screen pixel of a cell.
  static constexpr int16_t cellX(int col) { return static_cast<int16_t>(kBoardX + col * kCell); }
  static constexpr int16_t cellY(int row) { return static_cast<int16_t>(kBoardY + row * kCell); }

 private:
  static constexpr uint64_t cellBit(int row, int col) {
    return static_cast<uint64_t>(1) << (row * 8 + col);
  }
  static constexpr Rect clip(const Rect& r, const Rect& to) {
    if (r.empty()) return Rect{};
    const int16_t x0 = r.x > to.x ? r.x : to.x;
    const int16_t y0 = r.y > to.y ? r.y : to.y;
    const int16_t x1 = r.right() < to.right() ? r.right() : to.right();
    const int16_t y1 = r.bottom() < to.bottom() ? r.bottom() : to.bottom();
    if (x1 <= x0 || y1 <= y0) return Rect{};
    return Rect{x0, y0, static_cast<int16_t>(x1 - x0), static_cast<int16_t>(y1 - y0)};
  }

  uint64_t cells_ = 0;
  Rect rects_[kMaxRects];
  int count_ = 0;
};

}  // namespace checkers::ui
