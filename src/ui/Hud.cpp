#include "Hud.h"

#include <stdio.h>
#include <string.h>

namespace checkers::ui {

namespace {

constexpr int kBtnW = (kSidebarW - 2 * kPad - kButtonGap) / 2;  // 136
constexpr int kBtnRow1 = 248;
constexpr int kBtnRow2 = kBtnRow1 + kButtonH + kButtonGap;
constexpr int kTurnY = 168;
constexpr int kTurnH = 40;
constexpr int kStatusY = 212;
constexpr int kStatusH = 28;
constexpr int kTurnW = 170;  // turn text region; the clock takes the rest of the row

constexpr Rect kSidebar{kSidebarX, 0, kSidebarW, kScreenH};
constexpr Rect kRowRect[2] = {{kSidebarX, kPad, kSidebarW, kPlayerRowH},
                              {kSidebarX, kPad + kPlayerRowH, kSidebarW, kPlayerRowH}};
constexpr Rect kTurnRect{kSidebarX, kTurnY, kTurnW, kTurnH};
constexpr Rect kClockRect{kSidebarX + kTurnW, kTurnY, kSidebarW - kTurnW, kTurnH};
constexpr Rect kStatusRect{kSidebarX, kStatusY, kSidebarW, kStatusH};

const char* const kButtonLabel[3] = {"Undo", "Hint", "Menu"};
constexpr HudButton kButtons[3] = {HudButton::Undo, HudButton::Hint, HudButton::Menu};

bool timed(const HudModel& m) {
  return m.clockP1Sec != 0 || m.clockP2Sec != 0;
}
uint32_t turnClock(const HudModel& m) {
  return m.toMove == Side::Black ? m.clockP1Sec : m.clockP2Sec;
}
const char* turnInitials(const HudModel& m) {
  return m.toMove == Side::Black ? m.p1Initials : m.p2Initials;
}

void drawPlayerRow(LGFX& gfx, const Rect& row, Side side, const char* initials, int pieces,
                   int captured) {
  const int x0 = kSidebarX + kPad;
  const int mid = row.y + row.h / 2;
  gfx.fillRect(row.x, row.y, row.w, row.h, kHudBg);
  gfx.fillCircle(x0 + 14, mid, 12, side == Side::Black ? kP1Body : kP2Body);
  gfx.drawCircle(x0 + 14, mid, 12, side == Side::Black ? kP1Rim : kP2Rim);

  gfx.setTextColor(kHudText, kHudBg);
  gfx.setTextDatum(textdatum_t::middle_left);
  gfx.setFont(&fonts::FreeSansBold12pt7b);
  gfx.drawString(initials, x0 + 40, mid);

  char buf[24];
  snprintf(buf, sizeof(buf), "x%d", pieces);
  gfx.drawString(buf, x0 + 110, mid);

  snprintf(buf, sizeof(buf), "captured %d", captured);
  gfx.setTextColor(kHudTextMuted, kHudBg);
  gfx.setFont(&fonts::FreeSans12pt7b);
  gfx.setTextDatum(textdatum_t::middle_right);
  gfx.drawString(buf, kScreenW - kPad, mid);
}

}  // namespace

Rect Hud::buttonRect(HudButton b) {
  const int x1 = kSidebarX + kPad;
  const int x2 = x1 + kBtnW + kButtonGap;
  switch (b) {
    case HudButton::Undo:
      return Rect{static_cast<int16_t>(x1), kBtnRow1, kBtnW, kButtonH};
    case HudButton::Hint:
      return Rect{static_cast<int16_t>(x2), kBtnRow1, kBtnW, kButtonH};
    default:
      return Rect{static_cast<int16_t>(x1), kBtnRow2, kBtnW, kButtonH};
  }
}

bool Hud::enabled(const HudModel& m, HudButton b) {
  if (b == HudButton::Undo) return m.undoEnabled;
  if (b == HudButton::Hint) return m.hintEnabled;
  return true;
}

bool Hud::hitButton(int16_t x, int16_t y, HudButton& out) const {
  if (!model_.showButtons) return false;
  for (HudButton b : kButtons) {
    const Rect r = buttonRect(b);
    if (x >= r.x && x < r.right() && y >= r.y && y < r.bottom() && enabled(model_, b)) {
      out = b;
      return true;
    }
  }
  return false;
}

void Hud::invalidateAll() {
  renderer_.dirty().markRect(kSidebar);
}

void Hud::setModel(const HudModel& m) {
  HudModel next = m;
  char nextStatus[sizeof(statusBuf_)] = "";
  if (m.status) snprintf(nextStatus, sizeof(nextStatus), "%s", m.status);
  next.status = statusBuf_;

  DirtyRects& d = renderer_.dirty();
  const HudModel& o = model_;
  if (!haveModel_) {
    d.markRect(kSidebar);
  } else {
    if (strcmp(o.p1Initials, next.p1Initials) != 0 || o.p1Count != next.p1Count ||
        o.p1Captured != next.p1Captured) {
      d.markRect(kRowRect[0]);
    }
    if (strcmp(o.p2Initials, next.p2Initials) != 0 || o.p2Count != next.p2Count ||
        o.p2Captured != next.p2Captured) {
      d.markRect(kRowRect[1]);
    }
    if (o.toMove != next.toMove || strcmp(turnInitials(o), turnInitials(next)) != 0) {
      d.markRect(kTurnRect);
    }
    if (o.toMove != next.toMove || timed(o) != timed(next) || turnClock(o) != turnClock(next)) {
      d.markRect(kClockRect);
    }
    if (strcmp(statusBuf_, nextStatus) != 0) d.markRect(kStatusRect);
    for (HudButton b : kButtons) {
      if (o.showButtons != next.showButtons || enabled(o, b) != enabled(next, b)) {
        d.markRect(buttonRect(b));
      }
    }
  }
  model_ = next;
  memcpy(statusBuf_, nextStatus, sizeof(statusBuf_));
  haveModel_ = true;
}

void Hud::paintRect(LGFX& gfx, const Rect& r) {
  if (!r.intersects(kSidebar)) return;
  // The clip rectangle is r; fill the sidebar part first so gaps between regions are clean.
  gfx.fillRect(kSidebarX, 0, kSidebarW, kScreenH, kHudBg);
  if (!haveModel_) return;
  const HudModel& m = model_;

  if (r.intersects(kRowRect[0])) {
    drawPlayerRow(gfx, kRowRect[0], Side::Black, m.p1Initials, m.p1Count, m.p1Captured);
  }
  if (r.intersects(kRowRect[1])) {
    drawPlayerRow(gfx, kRowRect[1], Side::White, m.p2Initials, m.p2Count, m.p2Captured);
  }

  if (r.intersects(kTurnRect)) {
    char buf[24];
    snprintf(buf, sizeof(buf), "%s to move", turnInitials(m));
    gfx.fillRect(kSidebarX + kPad, kTurnY + 6, 4, 28, kHudAccent);
    gfx.setTextColor(kHudText, kHudBg);
    gfx.setTextDatum(textdatum_t::middle_left);
    gfx.setFont(&fonts::FreeSansBold12pt7b);
    gfx.drawString(buf, kSidebarX + kPad + 14, kTurnY + kTurnH / 2);
  }

  if (r.intersects(kClockRect) && timed(m)) {
    const uint32_t sec = turnClock(m);
    char buf[12];
    snprintf(buf, sizeof(buf), "%02u:%02u", (unsigned)((sec / 60) % 100), (unsigned)(sec % 60));
    gfx.setTextColor(kHudText, kHudBg);
    gfx.setTextDatum(textdatum_t::middle_right);
    gfx.setFont(&fonts::FreeSansBold24pt7b);
    gfx.drawString(buf, kScreenW - kPad, kTurnY + kTurnH / 2);
  }

  if (r.intersects(kStatusRect) && statusBuf_[0]) {
    gfx.setTextColor(kHudTextMuted, kHudBg);
    gfx.setTextDatum(textdatum_t::middle_left);
    gfx.setFont(&fonts::FreeSans12pt7b);
    gfx.drawString(statusBuf_, kSidebarX + kPad + 14, kStatusY + kStatusH / 2);
  }

  if (m.showButtons) {
    for (HudButton b : kButtons) {
      const Rect br = buttonRect(b);
      if (!r.intersects(br)) continue;
      const bool en = enabled(m, b);
      const uint16_t face = en ? kButtonFace : kButtonDisabled;
      gfx.fillRoundRect(br.x, br.y, br.w, br.h, 8, face);
      gfx.drawRoundRect(br.x, br.y, br.w, br.h, 8, kButtonBorder);
      gfx.setTextColor(en ? kButtonText : kButtonDisabledText, face);
      gfx.setTextDatum(textdatum_t::middle_center);
      gfx.setFont(&fonts::FreeSansBold12pt7b);
      gfx.drawString(kButtonLabel[static_cast<int>(b)], br.x + br.w / 2, br.y + br.h / 2);
    }
  }
}

}  // namespace checkers::ui
