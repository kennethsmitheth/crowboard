// Drops touch events that straddle a scene transition. No Arduino includes; host-tested in
// test/test_scenes.
//
// The touch driver queues Down / Move / Up / Tap, and a press that began on the previous scene
// can deliver its Up and Tap (or all of its events, if they were still queued) to the scene that
// just appeared: a Tap alone activates a widget (RegionPress::tap), so the second tap of a quick
// double tap on "Main Menu" used to land on whatever the menu placed under the finger. After
// reset() the gate swallows everything until a Down that happens at least kGraceMs after the
// transition; from then on every event passes. A press that started before the grace period
// ended is dropped as a whole (Down, Move, Up and Tap).
#pragma once

#include <stdint.h>

#include "../ui/TouchEvent.h"

namespace checkers::app {

class TouchGate {
 public:
  // The new screen is on the panel ~70 ms after the transition (full repaint); a deliberate tap
  // on it cannot come much sooner than a reaction time later.
  static constexpr uint32_t kGraceMs = 200;

  // Call when a transition was applied at nowMs (millis()).
  void reset(uint32_t nowMs) {
    since_ = nowMs;
    open_ = false;
  }

  // True when the event may reach the current scene.
  bool accept(const ui::TouchEvent& e) {
    if (open_) return true;
    if (e.type != ui::TouchType::Down) return false;
    if (static_cast<int32_t>(e.t - since_) < static_cast<int32_t>(kGraceMs)) return false;
    open_ = true;
    return true;
  }

  bool open() const { return open_; }

 private:
  uint32_t since_ = 0;
  bool open_ = true;  // before the first transition everything passes
};

}  // namespace checkers::app
