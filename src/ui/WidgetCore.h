// Arduino-free core of the widget set: hit-testing, press tracking, group dispatch, dirty
// collection and the initials buffer. Widgets.h adds painting (LovyanGFX) on top of this, so
// the logic can be unit-tested on the host (test/test_widgets).
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "DirtyRects.h"
#include "TouchEvent.h"

namespace checkers::ui {

constexpr bool hitTest(const Rect& r, int x, int y) {
  return x >= r.x && y >= r.y && x < r.right() && y < r.bottom();
}

// Copies at most n-1 chars and always terminates. Null source gives an empty string.
inline void copyText(char* dst, size_t n, const char* src) {
  size_t i = 0;
  if (src)
    for (; i + 1 < n && src[i]; ++i) dst[i] = src[i];
  dst[i] = '\0';
}

// Common base: geometry, visibility and the dirty flag. The dirty rect is the union of what was
// invalidated since the last collectDirty(); it defaults to the whole bounds.
class CoreWidget {
 public:
  virtual ~CoreWidget() = default;

  Rect bounds;
  bool visible = true;
  bool dirty = true;
  Rect dirtyRect;  // empty with dirty set means "all of bounds"

  // Returns true if the event was consumed.
  virtual bool handle(const TouchEvent&) { return false; }
  // A visible modal swallows every touch in the group, wherever it lands.
  virtual bool isModal() const { return false; }

  void setBounds(const Rect& r) {
    invalidate();  // old area
    bounds = r;
    invalidate();
  }
  void setVisible(bool v) {
    if (v == visible) return;
    visible = v;
    invalidate();
  }
  void invalidate() {
    dirty = true;
    dirtyRect = bounds;
  }
  void invalidate(const Rect& r) {
    dirty = true;
    dirtyRect = dirtyRect.empty() ? r : dirtyRect.unite(r);
  }
  bool contains(int x, int y) const { return hitTest(bounds, x, y); }
};

// Tracks one press over a set of numbered regions (a button is one region, a keypad has 29).
// Down remembers the region; the action fires on Up over the same region, or on a Tap that was
// not already preceded by a firing Up (the touch driver sends Up, then Tap).
class RegionPress {
 public:
  // Return true when the visual state (active()) changed.
  bool down(int region) {
    pressed_ = region;
    held_ = region >= 0;
    fired_ = false;
    return held_;
  }
  bool move(int region) {
    const bool h = pressed_ >= 0 && region == pressed_;
    const bool changed = h != held_;
    held_ = h;
    return changed;
  }
  // Region to activate, or -1.
  int up(int region) {
    const int r = (pressed_ >= 0 && region == pressed_) ? pressed_ : -1;
    if (r >= 0) fired_ = true;
    pressed_ = -1;
    held_ = false;
    return r;
  }
  int tap(int region) {
    if (fired_) {
      fired_ = false;
      return -1;
    }
    return region;
  }
  // Region currently shown as pressed, or -1.
  int active() const { return held_ ? pressed_ : -1; }
  void reset() {
    pressed_ = -1;
    held_ = false;
    fired_ = false;
  }

 private:
  int pressed_ = -1;
  bool held_ = false;
  bool fired_ = false;
};

// Fixed-capacity widget list; later additions are on top. No heap.
template <class W, int N = 16>
class GroupCore {
 public:
  static constexpr int kCapacity = N;

  bool add(W* w) {
    if (!w || count_ >= N) return false;
    w_[count_++] = w;
    return true;
  }
  int count() const { return count_; }
  W* at(int i) const { return (i >= 0 && i < count_) ? w_[i] : nullptr; }

  // Routes a touch. Down (and Tap) go to the topmost visible widget containing the point whose
  // handle() consumes it; a visible modal takes everything instead. Move and Up follow the
  // widget that took the Down. Returns true if some widget consumed the event.
  bool dispatch(const TouchEvent& e) {
    if (e.type == TouchType::Move || e.type == TouchType::Up) {
      W* a = active_;
      if (e.type == TouchType::Up) {
        active_ = nullptr;
        tapOwned_ = a != nullptr;  // the Tap that follows belongs to this press
      }
      if (!a || !a->visible) return false;
      a->handle(e);
      return true;
    }
    if (e.type == TouchType::Down) tapOwned_ = false;
    if (e.type == TouchType::Tap && tapOwned_) {
      tapOwned_ = false;  // already handled through Down/Up; avoids hitting what a hidden widget
      return true;        // (closed modal, keypad) uncovered
    }
    W* modal = topModal();
    if (modal) {
      if (e.type == TouchType::Down) active_ = modal;
      modal->handle(e);
      return true;
    }
    for (int i = count_ - 1; i >= 0; --i) {
      W* w = w_[i];
      if (!w->visible || !w->contains(e.x, e.y)) continue;
      if (w->handle(e)) {
        if (e.type == TouchType::Down) active_ = w;
        return true;
      }
    }
    if (e.type == TouchType::Down) active_ = nullptr;
    return false;
  }

  // Marks the dirty rect of every widget with its flag set (hidden ones too: they must be
  // erased) and clears the flags. Returns how many were collected.
  int collectDirty(DirtyRects& d) {
    int n = 0;
    for (int i = 0; i < count_; ++i) {
      W* w = w_[i];
      if (!w->dirty) continue;
      d.markRect(w->dirtyRect.empty() ? w->bounds : w->dirtyRect);
      w->dirty = false;
      w->dirtyRect = Rect{};
      ++n;
    }
    return n;
  }

  void invalidateAll() {
    for (int i = 0; i < count_; ++i) w_[i]->invalidate();
  }

 private:
  W* topModal() const {
    for (int i = count_ - 1; i >= 0; --i)
      if (w_[i]->visible && w_[i]->isModal()) return w_[i];
    return nullptr;
  }

  W* w_[N] = {};
  int count_ = 0;
  W* active_ = nullptr;
  bool tapOwned_ = false;
};

// Up to 3 letters A-Z.
class InitialsBuffer {
 public:
  static constexpr int kMax = 3;

  bool append(char c) {
    if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    if (c < 'A' || c > 'Z' || len_ >= kMax) return false;
    s_[len_++] = c;
    s_[len_] = '\0';
    return true;
  }
  bool backspace() {
    if (len_ == 0) return false;
    s_[--len_] = '\0';
    return true;
  }
  void clear() {
    len_ = 0;
    s_[0] = '\0';
  }
  int length() const { return len_; }
  bool full() const { return len_ == kMax; }
  const char* text() const { return s_; }

 private:
  char s_[kMax + 1] = {};
  int len_ = 0;
};

}  // namespace checkers::ui
