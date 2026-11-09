// Host tests for ui/Tween: easing, colour blend and PathTween.
#include <unity.h>

#include "ui/Tween.h"

using namespace checkers::ui;

void setUp(void) {}
void tearDown(void) {}

static void test_ease_endpoints_and_midpoint(void) {
  TEST_ASSERT_EQUAL_FLOAT(0.0f, easeInOutQuad(0.0f));
  TEST_ASSERT_EQUAL_FLOAT(0.5f, easeInOutQuad(0.5f));
  TEST_ASSERT_EQUAL_FLOAT(1.0f, easeInOutQuad(1.0f));
  TEST_ASSERT_EQUAL_FLOAT(0.0f, easeInOutQuad(-3.0f));
  TEST_ASSERT_EQUAL_FLOAT(1.0f, easeInOutQuad(7.0f));
}

static void test_ease_monotonic(void) {
  float prev = 0.0f;
  for (int i = 0; i <= 100; ++i) {
    const float v = easeInOutQuad(static_cast<float>(i) / 100.0f);
    TEST_ASSERT_TRUE(v >= prev);
    prev = v;
  }
}

static void test_blend565(void) {
  TEST_ASSERT_EQUAL_HEX16(0x1234, blend565(0x1234, 0xABCD, 0));
  TEST_ASSERT_EQUAL_HEX16(0xABCD, blend565(0x1234, 0xABCD, 255));
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, blend565(0xFFFF, 0xFFFF, 100));
}

static const Point kPts[3] = {{0, 0}, {120, 120}, {0, 240}};

static void test_endpoints_exact(void) {
  PathTween t(kPts, 3, 200);
  TEST_ASSERT_EQUAL_INT(2, t.segments());
  TEST_ASSERT_EQUAL_UINT32(400, t.duration());
  Point p = t.at(0);
  TEST_ASSERT_EQUAL_INT(0, p.x);
  TEST_ASSERT_EQUAL_INT(0, p.y);
  p = t.at(400);
  TEST_ASSERT_EQUAL_INT(0, p.x);
  TEST_ASSERT_EQUAL_INT(240, p.y);
  p = t.at(100000);
  TEST_ASSERT_EQUAL_INT(0, p.x);
  TEST_ASSERT_EQUAL_INT(240, p.y);
}

static void test_segment_boundaries(void) {
  PathTween t(kPts, 3, 200);
  int seg = -1;
  Point p = t.at(199, &seg);
  TEST_ASSERT_EQUAL_INT(0, seg);
  TEST_ASSERT_TRUE(p.x >= 118);
  p = t.at(200, &seg);  // exactly on the landing
  TEST_ASSERT_EQUAL_INT(1, seg);
  TEST_ASSERT_EQUAL_INT(120, p.x);
  TEST_ASSERT_EQUAL_INT(120, p.y);
  t.at(400, &seg);
  TEST_ASSERT_EQUAL_INT(1, seg);  // last segment once done
}

static void test_monotonic_progress_and_position(void) {
  const Point line[2] = {{10, 10}, {130, 10}};
  PathTween t(line, 2, 220);
  float prev = 0.0f;
  int prevX = 10;
  for (uint32_t ms = 0; ms <= 240; ++ms) {
    const float pr = t.progress(ms);
    TEST_ASSERT_TRUE(pr >= prev);
    prev = pr;
    const Point p = t.at(ms);
    TEST_ASSERT_TRUE(p.x >= prevX);
    prevX = p.x;
  }
  TEST_ASSERT_EQUAL_FLOAT(1.0f, t.progress(220));
}

static void test_done_after_total_duration(void) {
  PathTween t(kPts, 3, 220);
  TEST_ASSERT_FALSE(t.done(0));
  TEST_ASSERT_FALSE(t.done(439));
  TEST_ASSERT_TRUE(t.done(440));
  TEST_ASSERT_TRUE(t.done(1000));
}

static void test_zero_length_path(void) {
  const Point one[1] = {{60, 120}};
  PathTween t(one, 1, 220);
  TEST_ASSERT_EQUAL_INT(0, t.segments());
  TEST_ASSERT_TRUE(t.done(0));
  int seg = -1;
  const Point p = t.at(0, &seg);
  TEST_ASSERT_EQUAL_INT(60, p.x);
  TEST_ASSERT_EQUAL_INT(120, p.y);
  TEST_ASSERT_EQUAL_INT(0, seg);
  PathTween e;
  TEST_ASSERT_TRUE(e.done(0));
  TEST_ASSERT_EQUAL_INT(0, e.at(5).x);
}

static void test_clamps_to_max_points(void) {
  Point many[14];
  for (int i = 0; i < 14; ++i) many[i] = Point{static_cast<int16_t>(i * 10), 0};
  PathTween t(many, 14, 100);
  TEST_ASSERT_EQUAL_INT(PathTween::kMaxPoints, t.points());
  TEST_ASSERT_EQUAL_INT(100, t.at(100000).x);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_ease_endpoints_and_midpoint);
  RUN_TEST(test_ease_monotonic);
  RUN_TEST(test_blend565);
  RUN_TEST(test_endpoints_exact);
  RUN_TEST(test_segment_boundaries);
  RUN_TEST(test_monotonic_progress_and_position);
  RUN_TEST(test_done_after_total_duration);
  RUN_TEST(test_zero_length_path);
  RUN_TEST(test_clamps_to_max_points);
  return UNITY_END();
}
