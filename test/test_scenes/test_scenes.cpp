// Host tests for app/SceneStack: replace/push/pop ordering, depth limit, deferred transitions.
#include <unity.h>

#include <string>

#include "app/SceneStack.h"

using checkers::app::SceneStack;

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
  return UNITY_END();
}
