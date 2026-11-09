// Host tests for app/SceneStack (replace/push/pop ordering, depth limit, deferred transitions)
// and app/TouchGate (touches that straddle a transition).
#include <unity.h>

#include <string>

#include "app/SceneStack.h"
#include "app/TouchGate.h"

using checkers::app::SceneStack;
using checkers::app::TouchGate;
using checkers::ui::TouchEvent;
using checkers::ui::TouchType;

struct Dummy {
  const char* name;
};

static std::string trace;
using Stack = SceneStack<Dummy, 4>;

static void apply(Stack& s) {
  s.applyPending([](Dummy* d) { trace += std::string("x:") + d->name + " "; },
                 [](Dummy* d) { trace += std::string("e:") + d->name + " "; });
}

void setUp(void) {
  trace.clear();
}
void tearDown(void) {}

static void test_replace_on_empty_enters(void) {
  Stack s;
  Dummy a{"a"};
  s.requestReplace(&a);
  TEST_ASSERT_TRUE(s.hasPending());
  TEST_ASSERT_NULL(s.current());
  apply(s);
  TEST_ASSERT_EQUAL_PTR(&a, s.current());
  TEST_ASSERT_EQUAL_INT(1, s.depth());
  TEST_ASSERT_EQUAL_STRING("e:a ", trace.c_str());
}

static void test_replace_exits_then_enters(void) {
  Stack s;
  Dummy a{"a"}, b{"b"};
  s.requestReplace(&a);
  apply(s);
  trace.clear();
  s.requestReplace(&b);
  apply(s);
  TEST_ASSERT_EQUAL_STRING("x:a e:b ", trace.c_str());
  TEST_ASSERT_EQUAL_INT(1, s.depth());
  TEST_ASSERT_EQUAL_PTR(&b, s.current());
}

static void test_push_keeps_below_and_pop_reenters(void) {
  Stack s;
  Dummy a{"a"}, b{"b"};
  s.requestReplace(&a);
  s.requestPush(&b);
  apply(s);
  TEST_ASSERT_EQUAL_STRING("e:a e:b ", trace.c_str());
  TEST_ASSERT_EQUAL_INT(2, s.depth());
  TEST_ASSERT_EQUAL_PTR(&a, s.at(0));
  trace.clear();
  s.requestPop();
  apply(s);
  TEST_ASSERT_EQUAL_STRING("x:b e:a ", trace.c_str());
  TEST_ASSERT_EQUAL_PTR(&a, s.current());
}

static void test_last_scene_is_never_popped(void) {
  Stack s;
  Dummy a{"a"};
  s.requestReplace(&a);
  apply(s);
  trace.clear();
  s.requestPop();
  apply(s);
  TEST_ASSERT_EQUAL_STRING("", trace.c_str());
  TEST_ASSERT_EQUAL_PTR(&a, s.current());
  TEST_ASSERT_EQUAL_UINT32(1, s.dropped());
}

static void test_depth_limit_drops_push(void) {
  Stack s;
  Dummy d[5] = {{"0"}, {"1"}, {"2"}, {"3"}, {"4"}};
  s.requestReplace(&d[0]);
  s.requestPush(&d[1]);
  s.requestPush(&d[2]);
  s.requestPush(&d[3]);
  apply(s);
  TEST_ASSERT_EQUAL_INT(4, s.depth());
  s.requestPush(&d[4]);
  apply(s);
  TEST_ASSERT_EQUAL_INT(4, s.depth());
  TEST_ASSERT_EQUAL_PTR(&d[3], s.current());
  TEST_ASSERT_EQUAL_UINT32(1, s.dropped());
}

static void test_pending_queue_limit(void) {
  Stack s;
  Dummy a{"a"};
  for (int i = 0; i < Stack::kMaxPending; i++) TEST_ASSERT_TRUE(s.requestPush(&a));
  TEST_ASSERT_FALSE(s.requestPush(&a));
}

static void test_transition_requested_during_apply_waits_for_next(void) {
  Stack s;
  Dummy a{"a"}, b{"b"};
  s.requestReplace(&a);
  s.applyPending([](Dummy*) {}, [&](Dummy*) { s.requestReplace(&b); });
  TEST_ASSERT_EQUAL_PTR(&a, s.current());
  TEST_ASSERT_TRUE(s.hasPending());
  apply(s);
  TEST_ASSERT_EQUAL_PTR(&b, s.current());
}

// Mimics a frame: handle() requests a transition, which only takes effect at the next frame.
static void test_frame_defers_transition(void) {
  Stack s;
  Dummy a{"a"}, b{"b"};
  s.requestReplace(&a);
  apply(s);              // frame 1 start
  s.requestReplace(&b);  // requested during frame 1
  TEST_ASSERT_EQUAL_PTR(&a, s.current());
  apply(s);  // frame 2 start
  TEST_ASSERT_EQUAL_PTR(&b, s.current());
}

// ---- TouchGate ----------------------------------------------------------------------------------

static TouchEvent ev(TouchType type, uint32_t t) {
  return TouchEvent{type, 100, 100, t};
}

static void test_gate_passes_everything_before_any_transition(void) {
  TouchGate g;
  TEST_ASSERT_TRUE(g.open());
  TEST_ASSERT_TRUE(g.accept(ev(TouchType::Up, 10)));
  TEST_ASSERT_TRUE(g.accept(ev(TouchType::Tap, 10)));
  TEST_ASSERT_TRUE(g.accept(ev(TouchType::Move, 11)));
  TEST_ASSERT_TRUE(g.accept(ev(TouchType::Down, 12)));
}

// The Up and Tap of a press that began on the old scene arrive after the transition: dropped.
static void test_gate_drops_up_and_tap_without_a_new_down(void) {
  TouchGate g;
  g.reset(1000);
  TEST_ASSERT_FALSE(g.accept(ev(TouchType::Move, 1005)));
  TEST_ASSERT_FALSE(g.accept(ev(TouchType::Up, 1050)));
  TEST_ASSERT_FALSE(g.accept(ev(TouchType::Tap, 1050)));
  TEST_ASSERT_FALSE(g.accept(ev(TouchType::Tap, 1000 + TouchGate::kGraceMs + 500)));
  TEST_ASSERT_FALSE(g.open());
}

// A press that starts before the grace period is over is swallowed whole, even its late Up/Tap.
static void test_gate_drops_a_press_that_starts_too_early(void) {
  TouchGate g;
  g.reset(1000);
  TEST_ASSERT_FALSE(g.accept(ev(TouchType::Down, 1000 + TouchGate::kGraceMs - 1)));
  TEST_ASSERT_FALSE(g.accept(ev(TouchType::Move, 1000 + TouchGate::kGraceMs + 20)));
  TEST_ASSERT_FALSE(g.accept(ev(TouchType::Up, 1000 + TouchGate::kGraceMs + 80)));
  TEST_ASSERT_FALSE(g.accept(ev(TouchType::Tap, 1000 + TouchGate::kGraceMs + 80)));
  TEST_ASSERT_FALSE(g.open());
}

// The first Down after the grace period opens the gate for good (until the next transition).
static void test_gate_opens_on_a_down_after_the_grace_period(void) {
  TouchGate g;
  g.reset(1000);
  TEST_ASSERT_TRUE(g.accept(ev(TouchType::Down, 1000 + TouchGate::kGraceMs)));
  TEST_ASSERT_TRUE(g.open());
  TEST_ASSERT_TRUE(g.accept(ev(TouchType::Move, 1000 + TouchGate::kGraceMs + 30)));
  TEST_ASSERT_TRUE(g.accept(ev(TouchType::Up, 1000 + TouchGate::kGraceMs + 90)));
  TEST_ASSERT_TRUE(g.accept(ev(TouchType::Tap, 1000 + TouchGate::kGraceMs + 90)));
  g.reset(5000);  // next transition closes it again
  TEST_ASSERT_FALSE(g.open());
  TEST_ASSERT_FALSE(g.accept(ev(TouchType::Tap, 5010)));
}

// millis() wrap: a transition just before the wrap and a Down just after it.
static void test_gate_survives_millis_wrap(void) {
  TouchGate g;
  g.reset(0xFFFFFFF0u);
  TEST_ASSERT_FALSE(g.accept(ev(TouchType::Down, 0x00000010u)));         // 32 ms later: too early
  TEST_ASSERT_TRUE(g.accept(ev(TouchType::Down, TouchGate::kGraceMs)));  // 216 ms later
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_replace_on_empty_enters);
  RUN_TEST(test_replace_exits_then_enters);
  RUN_TEST(test_push_keeps_below_and_pop_reenters);
  RUN_TEST(test_last_scene_is_never_popped);
  RUN_TEST(test_depth_limit_drops_push);
  RUN_TEST(test_pending_queue_limit);
  RUN_TEST(test_transition_requested_during_apply_waits_for_next);
  RUN_TEST(test_frame_defers_transition);
  RUN_TEST(test_gate_passes_everything_before_any_transition);
  RUN_TEST(test_gate_drops_up_and_tap_without_a_new_down);
  RUN_TEST(test_gate_drops_a_press_that_starts_too_early);
  RUN_TEST(test_gate_opens_on_a_down_after_the_grace_period);
  RUN_TEST(test_gate_survives_millis_wrap);
  return UNITY_END();
}
