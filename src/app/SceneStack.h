// Scene stack plus pending-transition queue. No Arduino dependencies, no heap; host-testable.
//
// Transitions are only requested during a frame; applyPending() applies them in request order at
// the start of the next frame. replace() swaps the top (exit old, enter new), push() keeps the
// scene below alive without exit, pop() exits the top and re-enters the revealed scene so it can
// reinstall its painters and mark itself dirty. The last scene is never popped; a push beyond
// kDepth is dropped (counted in dropped()).
#pragma once

#include <stdint.h>

namespace checkers::app {

template <typename T, int kDepth = 4>
class SceneStack {
 public:
  static constexpr int kMaxPending = 4;

  // Queue a transition. Returns false when the pending queue is full.
  bool requestReplace(T* s) { return enqueue(Op::Replace, s); }
  bool requestPush(T* s) { return enqueue(Op::Push, s); }
  bool requestPop() { return enqueue(Op::Pop, nullptr); }

  bool hasPending() const { return pendingCount_ > 0; }
  int depth() const { return depth_; }
  T* current() const { return depth_ > 0 ? stack_[depth_ - 1] : nullptr; }
  T* at(int i) const { return (i >= 0 && i < depth_) ? stack_[i] : nullptr; }
  uint32_t dropped() const { return dropped_; }

  // Applies queued transitions. onExit(T*) and onEnter(T*) are called as described above.
  template <typename ExitFn, typename EnterFn>
  void applyPending(ExitFn onExit, EnterFn onEnter) {
    // Requests made from inside onExit/onEnter wait for the next call.
    const int n = pendingCount_;
    Pending ops[kMaxPending];
    for (int i = 0; i < n; i++) ops[i] = pending_[i];
    pendingCount_ = 0;
    for (int i = 0; i < n; i++) {
      switch (ops[i].op) {
        case Op::Replace:
          if (depth_ > 0) onExit(stack_[--depth_]);
          pushAndEnter(ops[i].scene, onEnter);
          break;
        case Op::Push:
          pushAndEnter(ops[i].scene, onEnter);
          break;
        case Op::Pop:
          if (depth_ > 1) {
            onExit(stack_[--depth_]);
            onEnter(stack_[depth_ - 1]);
          } else {
            dropped_++;
          }
          break;
      }
    }
  }

 private:
  enum class Op : uint8_t { Replace, Push, Pop };
  struct Pending {
    Op op = Op::Push;
    T* scene = nullptr;
  };

  bool enqueue(Op op, T* s) {
    if (pendingCount_ >= kMaxPending) return false;
    pending_[pendingCount_].op = op;
    pending_[pendingCount_].scene = s;
    pendingCount_++;
    return true;
  }

  template <typename EnterFn>
  void pushAndEnter(T* s, EnterFn& onEnter) {
    if (s == nullptr || depth_ >= kDepth) {
      dropped_++;
      return;
    }
    stack_[depth_++] = s;
    onEnter(s);
  }

  T* stack_[kDepth] = {};
  Pending pending_[kMaxPending];
  int depth_ = 0;
  int pendingCount_ = 0;
  uint32_t dropped_ = 0;
};

}  // namespace checkers::app
