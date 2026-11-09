// Debounce + tap detection state machine with a small event ring buffer.
// Pure logic: feed it one sample per poll, pop the resulting events. No Arduino dependencies.
#pragma once

#include <stdint.h>

#include "ui/TouchEvent.h"

namespace checkers::ui {

struct TapConfig {
  uint32_t debounceMs = 30;     // a state change must persist this long to be accepted
  uint32_t tapMaxMs = 400;      // Tap only if the press lasted less than this
  int16_t tapMaxMovePx = 12;    // ... and never strayed more than this from the Down point
  int16_t moveThresholdPx = 4;  // Move is emitted when the point moves more than this
  // Phantom-tap mitigation hooks (#45); the defaults leave behaviour unchanged. Both only gate
  // the Tap event: Down/Up/Move are still emitted.
  uint32_t minPressMs = 0;       // Tap only if the press lasted at least this long (0 = off)
  uint8_t maxTapsPerSecond = 0;  // at most this many Taps per rolling 1000 ms (0 = off, max 8)
};

class TapDetector {
 public:
  static constexpr uint8_t kQueueCapacity = 16;

  explicit TapDetector(const TapConfig& cfg = TapConfig()) : cfg_(cfg) {}

  void feed(bool pressed, int16_t x, int16_t y, uint32_t nowMs) {
    if (pressed != raw_) {
      raw_ = pressed;
      rawSince_ = nowMs;
    }
    if (pressed) {  // controllers report no coordinates on release; keep the last good point
      curX_ = x;
      curY_ = y;
    }
    if (raw_ != stable_) {
      if (nowMs - rawSince_ < cfg_.debounceMs) return;
      stable_ = raw_;
      if (stable_) {
        downX_ = lastX_ = curX_;
        downY_ = lastY_ = curY_;
        downT_ = nowMs;
        maxDistSq_ = 0;
        push({TouchType::Down, curX_, curY_, nowMs});
      } else {
        push({TouchType::Up, lastX_, lastY_, nowMs});
        const uint32_t pressMs = nowMs - downT_;
        if (pressMs < cfg_.tapMaxMs && pressMs >= cfg_.minPressMs &&
            maxDistSq_ <= (int32_t)cfg_.tapMaxMovePx * cfg_.tapMaxMovePx && tapAllowed(nowMs)) {
          push({TouchType::Tap, downX_, downY_, nowMs});
        }
      }
      return;
    }
    if (stable_ && pressed) {
      int32_t dd = distSq(curX_, curY_, downX_, downY_);
      if (dd > maxDistSq_) maxDistSq_ = dd;
      if (distSq(curX_, curY_, lastX_, lastY_) >
          (int32_t)cfg_.moveThresholdPx * cfg_.moveThresholdPx) {
        lastX_ = curX_;
        lastY_ = curY_;
        push({TouchType::Move, curX_, curY_, nowMs});
      }
    }
  }

  // Pops the oldest event. Returns false when empty.
  bool pop(TouchEvent& out) {
    if (count_ == 0) return false;
    out = q_[head_];
    head_ = (uint8_t)((head_ + 1) % kQueueCapacity);
    --count_;
    return true;
  }

  uint8_t size() const { return count_; }

 private:
  static int32_t distSq(int16_t ax, int16_t ay, int16_t bx, int16_t by) {
    int32_t dx = ax - bx, dy = ay - by;
    return dx * dx + dy * dy;
  }

  // Rolling rate limit: records the tap time if fewer than maxTapsPerSecond fall in the last 1 s.
  bool tapAllowed(uint32_t nowMs) {
    if (cfg_.maxTapsPerSecond == 0) return true;
    const uint8_t cap = cfg_.maxTapsPerSecond > kMaxRate ? kMaxRate : cfg_.maxTapsPerSecond;
    uint8_t recent = 0;
    for (uint8_t i = 0; i < tapCount_; ++i) {
      if (nowMs - tapTimes_[i] < 1000) ++recent;
    }
    if (recent >= cap) return false;
    if (tapCount_ == kMaxRate) {  // drop the oldest to make room
      for (uint8_t i = 1; i < tapCount_; ++i) tapTimes_[i - 1] = tapTimes_[i];
      --tapCount_;
    }
    tapTimes_[tapCount_++] = nowMs;
    return true;
  }

  // When full, the oldest event is dropped so the queue always holds the newest 16.
  void push(const TouchEvent& e) {
    if (count_ == kQueueCapacity) {
      head_ = (uint8_t)((head_ + 1) % kQueueCapacity);
      --count_;
    }
    q_[(head_ + count_) % kQueueCapacity] = e;
    ++count_;
  }

  TapConfig cfg_;
  TouchEvent q_[kQueueCapacity] = {};
  uint8_t head_ = 0, count_ = 0;
  bool raw_ = false, stable_ = false;
  uint32_t rawSince_ = 0, downT_ = 0;
  int16_t curX_ = 0, curY_ = 0, downX_ = 0, downY_ = 0, lastX_ = 0, lastY_ = 0;
  int32_t maxDistSq_ = 0;
  static constexpr uint8_t kMaxRate = 8;
  uint32_t tapTimes_[kMaxRate] = {};  // emitted Tap times, oldest first
  uint8_t tapCount_ = 0;
};

}  // namespace checkers::ui
