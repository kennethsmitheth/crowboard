// Minimal retained-mode touch widgets for the 800x480 panel: Button, Label, Toggle, ListView,
// Modal and an initials Keypad. Hit-testing, dispatch and dirty tracking live in WidgetCore.h
// (host-tested); this header adds painting with LovyanGFX fonts named in Theme.h.
//
// Usage (scenes):
//   static Button ok;  ok.init(x, y, w, h, "OK", onOk, this);
//   WidgetGroup g;     g.add(&ok);              // later additions are drawn and hit first
//   renderer.setRectPainter(&g);  g.invalidateAll();
//   loop:   g.dispatch(ev);  g.collectDirty(renderer.dirty());  renderer.flush();
//
// A widget changes its look by calling invalidate(); collectDirty() turns that into a renderer
// rectangle, so a press shows up on the next flush (within one frame). All text lives in fixed
// buffers; there is no heap use. Callbacks are plain function pointers with a void* context.
#pragma once

#include <stdint.h>

#include "../hal/crowpanel7_lgfx.h"
#include "Renderer.h"
#include "Theme.h"
#include "WidgetCore.h"

namespace checkers::ui {

class Widget : public CoreWidget {
 public:
  virtual void paint(LGFX& gfx) = 0;  // draw inside bounds; the clip rect is already set
};

// Widgets made of numbered touch regions (a button is one, a keypad has 28). Implements the
// Down/Move/Up/Tap sequence with press feedback; subclasses say what a region is.
class RegionWidget : public Widget {
 public:
  bool handle(const TouchEvent& e) override;

 protected:
  virtual int regionAt(int x, int y) const = 0;  // -1 for none
  virtual Rect regionRect(int region) const = 0;
  virtual void activate(int region) = 0;
  int pressedRegion() const { return press_.active(); }

 private:
  RegionPress press_;
};

class Button : public RegionWidget {
 public:
  using Action = void (*)(void* ctx);
  enum class State : uint8_t { Normal, Pressed, Disabled };

  void init(int16_t x, int16_t y, int16_t w, int16_t h, const char* label, Action cb = nullptr,
            void* ctx = nullptr);
  void setLabel(const char* label);
  void setAction(Action cb, void* ctx) {
    cb_ = cb;
    ctx_ = ctx;
  }
  void setEnabled(bool on);
  bool enabled() const { return enabled_; }
  State state() const;
  void paint(LGFX& gfx) override;

 protected:
  int regionAt(int x, int y) const override { return (enabled_ && contains(x, y)) ? 0 : -1; }
  Rect regionRect(int) const override { return bounds; }
  void activate(int) override {
    if (cb_) cb_(ctx_);
  }

 private:
  char label_[24] = {};
  Action cb_ = nullptr;
  void* ctx_ = nullptr;
  bool enabled_ = true;
};

class Label : public Widget {
 public:
  enum class Size : uint8_t { Body, Bold, Title };  // FreeSans12, FreeSansBold12, FreeSansBold24
  enum class Align : uint8_t { Left, Center, Right };

  void init(int16_t x, int16_t y, int16_t w, int16_t h, const char* text, Size size = Size::Body,
            Align align = Align::Left);
  void setText(const char* text);
  void setColor(uint16_t c) {
    color_ = c;
    invalidate();
  }
  const char* text() const { return text_; }
  void paint(LGFX& gfx) override;

 private:
  char text_[48] = {};
  Size size_ = Size::Body;
  Align align_ = Align::Left;
  uint16_t color_ = kHudText;
};

class Toggle : public RegionWidget {
 public:
  using Action = void (*)(void* ctx, bool on);

  void init(int16_t x, int16_t y, int16_t w, int16_t h, const char* label, bool on = false,
            Action cb = nullptr, void* ctx = nullptr);
  bool on() const { return on_; }
  void setOn(bool on);
  void paint(LGFX& gfx) override;

 protected:
  int regionAt(int x, int y) const override { return contains(x, y) ? 0 : -1; }
  Rect regionRect(int) const override { return bounds; }
  void activate(int) override;

 private:
  char label_[32] = {};
  Action cb_ = nullptr;
  void* ctx_ = nullptr;
  bool on_ = false;
};

// Scrolling list of caller-owned strings with up/down arrow buttons on the right. Rows are
// bounds.h / visibleRows tall (keep that >= 64). Tapping a row selects it.
class ListView : public RegionWidget {
 public:
  using Action = void (*)(void* ctx, int index);
  static constexpr int kMaxItems = 32;
  static constexpr int kArrowW = 72;

  void init(int16_t x, int16_t y, int16_t w, int16_t h, int visibleRows, Action cb = nullptr,
            void* ctx = nullptr);
  // The strings must outlive the list.
  void setItems(const char* const* items, int count);
  int count() const { return count_; }
  int selected() const { return selected_; }  // -1 for none
  void setSelected(int index);
  int top() const { return top_; }
  void paint(LGFX& gfx) override;

 protected:
  int regionAt(int x, int y) const override;
  Rect regionRect(int region) const override;
  void activate(int region) override;

 private:
  // Regions: 0 = up arrow, 1 = down arrow, 2 + r = visible row r.
  int maxTop() const { return count_ > rows_ ? count_ - rows_ : 0; }
  int rowH() const { return bounds.h / rows_; }

  const char* items_[kMaxItems] = {};
  int count_ = 0;
  int rows_ = 4;
  int top_ = 0;
  int selected_ = -1;
  Action cb_ = nullptr;
  void* ctx_ = nullptr;
};

// Centred bordered panel with a title, a message (use '\n' for a second line) and one or two
// buttons. While visible it captures every touch in its group. Add it to the group last.
class Modal : public RegionWidget {
 public:
  using Action = void (*)(void* ctx, int button);  // 0 = first button, 1 = second

  void init(int16_t x, int16_t y, int16_t w, int16_t h, Action cb = nullptr, void* ctx = nullptr);
  // Shows the modal; pass null for button2 to get a single button.
  void show(const char* title, const char* message, const char* button1,
            const char* button2 = nullptr);
  void hide() { setVisible(false); }
  bool isModal() const override { return true; }
  void paint(LGFX& gfx) override;

 protected:
  int regionAt(int x, int y) const override;
  Rect regionRect(int region) const override;
  void activate(int region) override;

 private:
  Rect buttonRect(int i) const;

  char title_[32] = {};
  char message_[96] = {};
  char button_[2][20] = {};
  int buttons_ = 1;
  Action cb_ = nullptr;
  void* ctx_ = nullptr;
};

// 3-letter initials entry: A-Z in a 7x4 grid, Del and OK in the last row, current initials in
// slots on top. Regions: 0..25 letters, 26 Del, 27 OK (enabled once one letter is entered).
// Pass the full screen (or any rect >= 640x420) as bounds; keys come out >= 64 px.
class Keypad : public RegionWidget {
 public:
  using Action = void (*)(void* ctx, const char* initials);
  static constexpr int kCols = 7;
  static constexpr int kRows = 4;
  static constexpr int kKeyDel = 26;
  static constexpr int kKeyOk = 27;
  static constexpr int kTopH = 96;  // initials display height
  static constexpr int kPad = 16;
  static constexpr int kGap = 8;

  void init(int16_t x, int16_t y, int16_t w, int16_t h, Action onOk = nullptr, void* ctx = nullptr);
  const char* text() const { return buf_.text(); }
  void setText(const char* s);
  void clear() {
    buf_.clear();
    invalidate();
  }
  Rect keyRect(int key) const;
  void paint(LGFX& gfx) override;

 protected:
  int regionAt(int x, int y) const override;
  Rect regionRect(int region) const override { return keyRect(region); }
  void activate(int region) override;

 private:
  InitialsBuffer buf_;
  Action cb_ = nullptr;
  void* ctx_ = nullptr;
};

// Owns nothing; holds up to 16 widgets. Doubles as the Renderer's RectPainter.
class WidgetGroup : public RectPainter, public GroupCore<Widget, 16> {
 public:
  uint16_t background = kHudBg;

  // Fills the rect with the background and paints every visible widget that intersects it.
  void paintRect(LGFX& gfx, const Rect& r) override;
};

}  // namespace checkers::ui
