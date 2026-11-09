#include "Widgets.h"

#include <string.h>

namespace checkers::ui {
namespace {

constexpr uint16_t kPanelFill = rgb565(54, 48, 44);
constexpr uint16_t kOnFill = rgb565(76, 175, 80);
constexpr uint16_t kKnob = rgb565(245, 240, 230);
constexpr uint16_t kDarkText = rgb565(30, 26, 24);
constexpr int kRadius = 10;

const lgfx::IFont* bodyFont() {
  return &fonts::FreeSans12pt7b;
}
const lgfx::IFont* boldFont() {
  return &fonts::FreeSansBold12pt7b;
}
const lgfx::IFont* titleFont() {
  return &fonts::FreeSansBold24pt7b;
}

void drawText(LGFX& gfx, const char* s, const Rect& r, const lgfx::IFont* font, uint16_t color,
              textdatum_t datum) {
  gfx.setFont(font);
  gfx.setTextColor(color);
  gfx.setTextDatum(datum);
  int x = r.x + r.w / 2;
  if (datum == textdatum_t::middle_left) x = r.x;
  if (datum == textdatum_t::middle_right) x = r.right();
  gfx.drawString(s, x, r.y + r.h / 2);
}

// Rounded key/button face in one of the three states.
void drawFace(LGFX& gfx, const Rect& r, const char* label, Button::State st,
              const lgfx::IFont* font) {
  const uint16_t face = st == Button::State::Pressed    ? kButtonPressed
                        : st == Button::State::Disabled ? kButtonDisabled
                                                        : kButtonFace;
  const uint16_t text = st == Button::State::Disabled ? kButtonDisabledText : kButtonText;
  gfx.fillRoundRect(r.x, r.y, r.w, r.h, kRadius, face);
  gfx.drawRoundRect(r.x, r.y, r.w, r.h, kRadius, kButtonBorder);
  if (st == Button::State::Pressed) {  // inset look: second inner border
    gfx.drawRoundRect(r.x + 1, r.y + 1, r.w - 2, r.h - 2, kRadius, kHudAccent);
  }
  drawText(gfx, label, r, font, text, textdatum_t::middle_center);
}

}  // namespace

// ---- RegionWidget -----------------------------------------------------------------------------

bool RegionWidget::handle(const TouchEvent& e) {
  const int r = regionAt(e.x, e.y);
  switch (e.type) {
    case TouchType::Down:
      if (press_.down(r)) invalidate(regionRect(r));
      return contains(e.x, e.y) || isModal();
    case TouchType::Move: {
      const int prev = press_.active();
      press_.move(r);
      const int cur = press_.active();
      if (prev != cur) invalidate(regionRect(cur >= 0 ? cur : prev));
      return true;
    }
    case TouchType::Up: {
      const int prev = press_.active();
      const int a = press_.up(r);
      if (prev >= 0) invalidate(regionRect(prev));
      if (a >= 0) activate(a);
      return true;
    }
    case TouchType::Tap: {
      const int a = press_.tap(r);
      if (a >= 0) activate(a);
      return true;
    }
  }
  return false;
}

// ---- Button -----------------------------------------------------------------------------------

void Button::init(int16_t x, int16_t y, int16_t w, int16_t h, const char* label, Action cb,
                  void* ctx) {
  setBounds(Rect{x, y, w, h});
  copyText(label_, sizeof label_, label);
  cb_ = cb;
  ctx_ = ctx;
}

void Button::setLabel(const char* label) {
  if (strncmp(label_, label ? label : "", sizeof label_ - 1) == 0) return;
  copyText(label_, sizeof label_, label);
  invalidate();
}

void Button::setEnabled(bool on) {
  if (on == enabled_) return;
  enabled_ = on;
  invalidate();
}

Button::State Button::state() const {
  if (!enabled_) return State::Disabled;
  return pressedRegion() == 0 ? State::Pressed : State::Normal;
}

void Button::paint(LGFX& gfx) {
  drawFace(gfx, bounds, label_, state(), boldFont());
}

// ---- Label ------------------------------------------------------------------------------------

void Label::init(int16_t x, int16_t y, int16_t w, int16_t h, const char* text, Size size,
                 Align align) {
  setBounds(Rect{x, y, w, h});
  copyText(text_, sizeof text_, text);
  size_ = size;
  align_ = align;
}

void Label::setText(const char* text) {
  if (strncmp(text_, text ? text : "", sizeof text_ - 1) == 0) return;
  copyText(text_, sizeof text_, text);
  invalidate();
}

void Label::paint(LGFX& gfx) {
  const lgfx::IFont* f = size_ == Size::Title  ? titleFont()
                         : size_ == Size::Bold ? boldFont()
                                               : bodyFont();
  const textdatum_t d = align_ == Align::Left     ? textdatum_t::middle_left
                        : align_ == Align::Center ? textdatum_t::middle_center
                                                  : textdatum_t::middle_right;
  drawText(gfx, text_, bounds, f, color_, d);
}

// ---- Toggle -----------------------------------------------------------------------------------

void Toggle::init(int16_t x, int16_t y, int16_t w, int16_t h, const char* label, bool on, Action cb,
                  void* ctx) {
  setBounds(Rect{x, y, w, h});
  copyText(label_, sizeof label_, label);
  on_ = on;
  cb_ = cb;
  ctx_ = ctx;
}

void Toggle::setOn(bool on) {
  if (on == on_) return;
  on_ = on;
  invalidate();
}

void Toggle::activate(int) {
  on_ = !on_;
  invalidate();
  if (cb_) cb_(ctx_, on_);
}

void Toggle::paint(LGFX& gfx) {
  const bool pressed = pressedRegion() == 0;
  gfx.fillRoundRect(bounds.x, bounds.y, bounds.w, bounds.h, kRadius,
                    pressed ? kButtonPressed : kButtonFace);
  gfx.drawRoundRect(bounds.x, bounds.y, bounds.w, bounds.h, kRadius, kButtonBorder);
  drawText(gfx, label_,
           Rect{static_cast<int16_t>(bounds.x + 16), bounds.y, static_cast<int16_t>(bounds.w - 128),
                bounds.h},
           boldFont(), kButtonText, textdatum_t::middle_left);
  const int pw = 88, ph = 44;
  const int px = bounds.right() - 16 - pw;
  const int py = bounds.y + (bounds.h - ph) / 2;
  gfx.fillRoundRect(px, py, pw, ph, ph / 2, on_ ? kOnFill : kButtonDisabled);
  gfx.drawRoundRect(px, py, pw, ph, ph / 2, kButtonBorder);
  const int kr = ph / 2 - 5;
  const int kx = on_ ? px + pw - ph / 2 : px + ph / 2;
  gfx.fillCircle(kx, py + ph / 2, kr, kKnob);
}

// ---- ListView ---------------------------------------------------------------------------------

void ListView::init(int16_t x, int16_t y, int16_t w, int16_t h, int visibleRows, Action cb,
                    void* ctx) {
  setBounds(Rect{x, y, w, h});
  rows_ = visibleRows < 1 ? 1 : visibleRows;
  cb_ = cb;
  ctx_ = ctx;
}

void ListView::setItems(const char* const* items, int count) {
  count_ = count > kMaxItems ? kMaxItems : (count < 0 ? 0 : count);
  for (int i = 0; i < count_; ++i) items_[i] = items[i];
  top_ = 0;
  selected_ = -1;
  invalidate();
}

void ListView::setSelected(int index) {
  if (index < -1 || index >= count_ || index == selected_) return;
  selected_ = index;
  if (index >= 0) {
    if (index < top_) top_ = index;
    if (index >= top_ + rows_) top_ = index - rows_ + 1;
  }
  invalidate();
}

Rect ListView::regionRect(int region) const {
  if (region == 0)
    return Rect{static_cast<int16_t>(bounds.right() - kArrowW), bounds.y, kArrowW, kButtonH};
  if (region == 1)
    return Rect{static_cast<int16_t>(bounds.right() - kArrowW),
                static_cast<int16_t>(bounds.bottom() - kButtonH), kArrowW, kButtonH};
  const int r = region - 2;
  return Rect{bounds.x, static_cast<int16_t>(bounds.y + r * rowH()),
              static_cast<int16_t>(bounds.w - kArrowW - 8), static_cast<int16_t>(rowH())};
}

int ListView::regionAt(int x, int y) const {
  if (top_ > 0 && hitTest(regionRect(0), x, y)) return 0;
  if (top_ < maxTop() && hitTest(regionRect(1), x, y)) return 1;
  for (int r = 0; r < rows_ && top_ + r < count_; ++r)
    if (hitTest(regionRect(2 + r), x, y)) return 2 + r;
  return -1;
}

void ListView::activate(int region) {
  if (region == 0 || region == 1) {
    const int nt = top_ + (region == 0 ? -1 : 1);
    top_ = nt < 0 ? 0 : (nt > maxTop() ? maxTop() : nt);
    invalidate();
    return;
  }
  const int index = top_ + region - 2;
  if (index >= count_) return;
  selected_ = index;
  invalidate();
  if (cb_) cb_(ctx_, index);
}

void ListView::paint(LGFX& gfx) {
  for (int r = 0; r < rows_; ++r) {
    const Rect rr = regionRect(2 + r);
    const int index = top_ + r;
    const bool has = index < count_;
    const bool sel = has && index == selected_;
    const bool pressed = pressedRegion() == 2 + r;
    gfx.fillRect(rr.x, rr.y, rr.w, rr.h, kHudBg);
    if (!has) continue;
    gfx.fillRoundRect(rr.x + 2, rr.y + 2, rr.w - 4, rr.h - 4, 8,
                      sel ? kSelect : (pressed ? kButtonPressed : kButtonFace));
    gfx.drawRoundRect(rr.x + 2, rr.y + 2, rr.w - 4, rr.h - 4, 8, kButtonBorder);
    drawText(gfx, items_[index],
             Rect{static_cast<int16_t>(rr.x + 16), rr.y, static_cast<int16_t>(rr.w - 32), rr.h},
             boldFont(), sel ? kDarkText : kButtonText, textdatum_t::middle_left);
  }
  for (int a = 0; a < 2; ++a) {
    const Rect ar = regionRect(a);
    const bool ok = a == 0 ? top_ > 0 : top_ < maxTop();
    const Button::State st = !ok                    ? Button::State::Disabled
                             : pressedRegion() == a ? Button::State::Pressed
                                                    : Button::State::Normal;
    drawFace(gfx, ar, "", st, boldFont());
    const uint16_t c = ok ? kButtonText : kButtonDisabledText;
    const int cx = ar.x + ar.w / 2, cy = ar.y + ar.h / 2;
    if (a == 0)
      gfx.fillTriangle(cx - 14, cy + 8, cx + 14, cy + 8, cx, cy - 10, c);
    else
      gfx.fillTriangle(cx - 14, cy - 8, cx + 14, cy - 8, cx, cy + 10, c);
  }
}

// ---- Modal ------------------------------------------------------------------------------------

void Modal::init(int16_t x, int16_t y, int16_t w, int16_t h, Action cb, void* ctx) {
  setBounds(Rect{x, y, w, h});
  cb_ = cb;
  ctx_ = ctx;
  visible = false;
}

void Modal::show(const char* title, const char* message, const char* button1, const char* button2) {
  copyText(title_, sizeof title_, title);
  copyText(message_, sizeof message_, message);
  copyText(button_[0], sizeof button_[0], button1);
  copyText(button_[1], sizeof button_[1], button2);
  buttons_ = button2 ? 2 : 1;
  visible = true;
  invalidate();
}

Rect Modal::buttonRect(int i) const {
  const int bw = buttons_ == 1 ? 200 : 180;
  const int gap = 24;
  const int total = buttons_ * bw + (buttons_ - 1) * gap;
  const int x0 = bounds.x + (bounds.w - total) / 2;
  return Rect{static_cast<int16_t>(x0 + i * (bw + gap)),
              static_cast<int16_t>(bounds.bottom() - 24 - kButtonH), static_cast<int16_t>(bw),
              kButtonH};
}

Rect Modal::regionRect(int region) const {
  return buttonRect(region);
}

int Modal::regionAt(int x, int y) const {
  for (int i = 0; i < buttons_; ++i)
    if (hitTest(buttonRect(i), x, y)) return i;
  return -1;
}

void Modal::activate(int region) {
  hide();
  if (cb_) cb_(ctx_, region);
}

void Modal::paint(LGFX& gfx) {
  gfx.fillRoundRect(bounds.x, bounds.y, bounds.w, bounds.h, 16, kPanelFill);
  for (int i = 0; i < 3; ++i)
    gfx.drawRoundRect(bounds.x + i, bounds.y + i, bounds.w - 2 * i, bounds.h - 2 * i, 16 - i,
                      kSelect);
  drawText(gfx, title_, Rect{bounds.x, static_cast<int16_t>(bounds.y + 16), bounds.w, 48},
           boldFont(), kSelect, textdatum_t::middle_center);
  // Message: one or two lines split at '\n', centred between the title and the buttons.
  char line[48];
  const char* p = message_;
  int n = 0;
  const char* q = p;
  while (*q) {
    if (*q == '\n') ++n;
    ++q;
  }
  const int lines = n >= 1 ? 2 : 1;
  const int areaTop = bounds.y + 72;
  const int areaH = buttonRect(0).y - 8 - areaTop;
  for (int i = 0; i < lines; ++i) {
    const char* nl = strchr(p, '\n');
    const size_t len = nl ? static_cast<size_t>(nl - p) : strlen(p);
    const size_t m = len < sizeof line - 1 ? len : sizeof line - 1;
    memcpy(line, p, m);
    line[m] = '\0';
    const int lh = 32;
    const int y = areaTop + areaH / 2 - (lines * lh) / 2 + i * lh;
    drawText(gfx, line, Rect{bounds.x, static_cast<int16_t>(y), bounds.w, lh}, bodyFont(), kHudText,
             textdatum_t::middle_center);
    if (!nl) break;
    p = nl + 1;
  }
  for (int i = 0; i < buttons_; ++i)
    drawFace(gfx, buttonRect(i), button_[i],
             pressedRegion() == i ? Button::State::Pressed : Button::State::Normal, boldFont());
}

// ---- Keypad -----------------------------------------------------------------------------------

void Keypad::init(int16_t x, int16_t y, int16_t w, int16_t h, Action onOk, void* ctx) {
  setBounds(Rect{x, y, w, h});
  cb_ = onOk;
  ctx_ = ctx;
}

void Keypad::setText(const char* s) {
  buf_.clear();
  for (; s && *s; ++s) buf_.append(*s);
  invalidate();
}

Rect Keypad::keyRect(int key) const {
  const int kw = (bounds.w - 2 * kPad - (kCols - 1) * kGap) / kCols;
  const int kh = (bounds.h - kTopH - kPad - (kRows - 1) * kGap) / kRows;
  const int col = key % kCols, row = key / kCols;
  return Rect{static_cast<int16_t>(bounds.x + kPad + col * (kw + kGap)),
              static_cast<int16_t>(bounds.y + kTopH + row * (kh + kGap)), static_cast<int16_t>(kw),
              static_cast<int16_t>(kh)};
}

int Keypad::regionAt(int x, int y) const {
  for (int k = 0; k < 28; ++k) {
    if (!hitTest(keyRect(k), x, y)) continue;
    if (k == kKeyOk && buf_.length() == 0) return -1;
    return k;
  }
  return -1;
}

void Keypad::activate(int region) {
  if (region == kKeyDel) {
    if (buf_.backspace()) invalidate(Rect{bounds.x, bounds.y, bounds.w, kTopH});
    invalidate(keyRect(kKeyOk));  // OK enable state may change
  } else if (region == kKeyOk) {
    if (cb_) cb_(ctx_, buf_.text());
  } else if (region >= 0 && region < 26) {
    if (buf_.append(static_cast<char>('A' + region))) {
      invalidate(Rect{bounds.x, bounds.y, bounds.w, kTopH});
      invalidate(keyRect(kKeyOk));
    }
  }
}

void Keypad::paint(LGFX& gfx) {
  gfx.fillRect(bounds.x, bounds.y, bounds.w, bounds.h, kHudBg);
  // Initials slots.
  const int sw = 80, sh = 72, sg = 16;
  const int sx = bounds.x + (bounds.w - (3 * sw + 2 * sg)) / 2;
  for (int i = 0; i < InitialsBuffer::kMax; ++i) {
    const Rect s{static_cast<int16_t>(sx + i * (sw + sg)), static_cast<int16_t>(bounds.y + 12),
                 static_cast<int16_t>(sw), static_cast<int16_t>(sh)};
    const bool filled = i < buf_.length();
    gfx.fillRoundRect(s.x, s.y, s.w, s.h, kRadius, kPanelFill);
    gfx.drawRoundRect(s.x, s.y, s.w, s.h, kRadius, filled ? kSelect : kButtonBorder);
    char c[2] = {filled ? buf_.text()[i] : '_', '\0'};
    drawText(gfx, c, s, titleFont(), filled ? kHudText : kHudTextMuted, textdatum_t::middle_center);
  }
  for (int k = 0; k < 28; ++k) {
    char label[4] = {};
    if (k < 26) {
      label[0] = static_cast<char>('A' + k);
    } else {
      copyText(label, sizeof label, k == kKeyDel ? "Del" : "OK");
    }
    const bool disabled = k == kKeyOk && buf_.length() == 0;
    drawFace(gfx, keyRect(k), label,
             disabled               ? Button::State::Disabled
             : pressedRegion() == k ? Button::State::Pressed
                                    : Button::State::Normal,
             boldFont());
  }
}

// ---- WidgetGroup ------------------------------------------------------------------------------

void WidgetGroup::paintRect(LGFX& gfx, const Rect& r) {
  gfx.fillRect(r.x, r.y, r.w, r.h, background);
  for (int i = 0; i < count(); ++i) {
    Widget* w = at(i);
    if (w->visible && w->bounds.intersects(r)) w->paint(gfx);
  }
}

}  // namespace checkers::ui
