// Host tests for the touch input logic (TapDetector, BoardGeometry).
#include <unity.h>

#include "ui/BoardGeometry.h"
#include "ui/TapDetector.h"

using namespace checkers::ui;

void setUp(void) {}
void tearDown(void) {}

static int drain(TapDetector& d, TouchEvent* out, int max = 32) {
  int n = 0;
  while (n < max && d.pop(out[n])) ++n;
  return n;
}

static void test_debounce_swallows_glitch(void) {
  TapDetector d;
  d.feed(true, 100, 100, 0);
  d.feed(true, 100, 100, 5);
  d.feed(false, 0, 0, 10);
  for (uint32_t t = 15; t < 200; t += 5) d.feed(false, 0, 0, t);
  TouchEvent e;
  TEST_ASSERT_FALSE(d.pop(e));
}

static void test_short_press_down_up_tap(void) {
  TapDetector d;
  for (uint32_t t = 0; t <= 100; t += 10) d.feed(true, 200, 150, t);
  for (uint32_t t = 110; t <= 200; t += 10) d.feed(false, 0, 0, t);
  TouchEvent ev[8];
  int n = drain(d, ev, 8);
  TEST_ASSERT_EQUAL_INT(3, n);
  TEST_ASSERT_EQUAL_UINT8((uint8_t)TouchType::Down, (uint8_t)ev[0].type);
  TEST_ASSERT_EQUAL_UINT8((uint8_t)TouchType::Up, (uint8_t)ev[1].type);
  TEST_ASSERT_EQUAL_UINT8((uint8_t)TouchType::Tap, (uint8_t)ev[2].type);
  TEST_ASSERT_EQUAL_INT16(200, ev[1].x);  // Up carries the last known point
  TEST_ASSERT_EQUAL_INT16(150, ev[2].y);
}

static void test_long_press_no_tap(void) {
  TapDetector d;
  for (uint32_t t = 0; t <= 600; t += 10) d.feed(true, 50, 50, t);
  for (uint32_t t = 610; t <= 700; t += 10) d.feed(false, 0, 0, t);
  TouchEvent ev[8];
  int n = drain(d, ev, 8);
  TEST_ASSERT_EQUAL_INT(2, n);
  TEST_ASSERT_EQUAL_UINT8((uint8_t)TouchType::Down, (uint8_t)ev[0].type);
  TEST_ASSERT_EQUAL_UINT8((uint8_t)TouchType::Up, (uint8_t)ev[1].type);
}

static void test_drag_no_tap(void) {
  TapDetector d;
  int16_t x = 100;
  for (uint32_t t = 0; t <= 200; t += 10) {
    d.feed(true, x, 100, t);
    if (t >= 50) x += 3;  // 3 px per poll; well past 12 px by the end
  }
  for (uint32_t t = 210; t <= 300; t += 10) d.feed(false, 0, 0, t);
  TouchEvent ev[64];
  int n = drain(d, ev, 64);
  int moves = 0;
  for (int i = 0; i < n; ++i) {
    TEST_ASSERT_TRUE(ev[i].type != TouchType::Tap);
    if (ev[i].type == TouchType::Move) ++moves;
  }
  TEST_ASSERT_TRUE(moves > 0);
  TEST_ASSERT_EQUAL_UINT8((uint8_t)TouchType::Up, (uint8_t)ev[n - 1].type);
}

static void test_ring_overflow_drops_oldest(void) {
  TapDetector d;
  uint32_t t = 0;
  // Each quick press-release produces Down, Up, Tap = 3 events; 8 taps = 24 > 16.
  for (int i = 0; i < 8; ++i) {
    for (int k = 0; k < 8; ++k, t += 10) d.feed(true, (int16_t)(10 * i), 20, t);
    for (int k = 0; k < 8; ++k, t += 10) d.feed(false, 0, 0, t);
  }
  TEST_ASSERT_EQUAL_UINT8(16, d.size());
  TouchEvent ev[32];
  int n = drain(d, ev);
  TEST_ASSERT_EQUAL_INT(16, n);
  // Newest events survive: the final one is the Tap of the last press (x = 70).
  TEST_ASSERT_EQUAL_UINT8((uint8_t)TouchType::Tap, (uint8_t)ev[15].type);
  TEST_ASSERT_EQUAL_INT16(70, ev[15].x);
  for (int i = 1; i < n; ++i) TEST_ASSERT_TRUE(ev[i].t >= ev[i - 1].t);
}

static void test_cell_corners(void) {
  TEST_ASSERT_EQUAL_INT(0, cellAt(0, 0));       // top-left is light
  TEST_ASSERT_EQUAL_INT(4, cellAt(479, 0));     // top-right dark corner
  TEST_ASSERT_EQUAL_INT(29, cellAt(0, 479));    // bottom-left dark corner
  TEST_ASSERT_EQUAL_INT(0, cellAt(479, 479));   // bottom-right is light
  TEST_ASSERT_EQUAL_INT(1, cellAt(60, 0));      // first dark square of row 0
  TEST_ASSERT_EQUAL_INT(32, cellAt(360, 420));  // last dark square of row 7
}

static void test_cell_light_and_outside(void) {
  TEST_ASSERT_EQUAL_INT(0, cellAt(30, 30));
  TEST_ASSERT_EQUAL_INT(0, cellAt(90, 90));
  TEST_ASSERT_EQUAL_INT(5, cellAt(30, 90));
  TEST_ASSERT_EQUAL_INT(0, cellAt(-1, 100));
  TEST_ASSERT_EQUAL_INT(0, cellAt(100, -1));
  TEST_ASSERT_EQUAL_INT(0, cellAt(480, 60));
  TEST_ASSERT_EQUAL_INT(0, cellAt(60, 480));
  TEST_ASSERT_EQUAL_INT(0, cellAt(700, 200));
}

static void test_cell_rect_roundtrip(void) {
  for (int s = 1; s <= 32; ++s) {
    int16_t x, y;
    cellRect(s, x, y);
    TEST_ASSERT_EQUAL_INT(s, cellAt(x + 30, y + 30));
  }
}

static int countType(TouchEvent* ev, int n, TouchType t) {
  int c = 0;
  for (int i = 0; i < n; ++i) c += ev[i].type == t;
  return c;
}

// Press for pressMs starting at t0, then release; returns the time after the release.
static uint32_t press(TapDetector& d, uint32_t t0, uint32_t pressMs) {
  uint32_t t = t0;
  for (; t <= t0 + pressMs; t += 5) d.feed(true, 100, 100, t);
  for (uint32_t end = t + 60; t < end; t += 5) d.feed(false, 0, 0, t);
  return t;
}

static void test_defaults_leave_taps_unchanged(void) {
  TapConfig c;
  TEST_ASSERT_EQUAL_UINT32(0, c.minPressMs);
  TEST_ASSERT_EQUAL_UINT8(0, c.maxTapsPerSecond);
  TapDetector d;
  uint32_t t = 0;
  for (int i = 0; i < 6; ++i) t = press(d, t, 40);  // many quick taps, none suppressed
  TouchEvent ev[32];
  TEST_ASSERT_EQUAL_INT(6, countType(ev, drain(d, ev, 32), TouchType::Tap));
}

static void test_min_press_ms_suppresses_short_tap_only(void) {
  TapConfig c;
  c.minPressMs = 120;
  TapDetector d(c);
  uint32_t t = press(d, 0, 60);  // debounced press well under 120 ms
  TouchEvent ev[8];
  int n = drain(d, ev, 8);
  TEST_ASSERT_EQUAL_INT(0, countType(ev, n, TouchType::Tap));
  TEST_ASSERT_EQUAL_INT(1, countType(ev, n, TouchType::Down));  // Down/Up still reported
  TEST_ASSERT_EQUAL_INT(1, countType(ev, n, TouchType::Up));
  press(d, t, 200);  // long enough
  n = drain(d, ev, 8);
  TEST_ASSERT_EQUAL_INT(1, countType(ev, n, TouchType::Tap));
}

static void test_max_taps_per_second_rate_limit(void) {
  TapConfig c;
  c.maxTapsPerSecond = 2;
  TapDetector d(c);
  uint32_t t = 0;
  for (int i = 0; i < 4; ++i) t = press(d, t, 40);  // ~110 ms apart: 4 attempts inside 1 s
  TouchEvent ev[32];
  TEST_ASSERT_EQUAL_INT(2, countType(ev, drain(d, ev, 32), TouchType::Tap));
  t += 1000;  // the window has rolled over
  press(d, t, 40);
  TEST_ASSERT_EQUAL_INT(1, countType(ev, drain(d, ev, 32), TouchType::Tap));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_debounce_swallows_glitch);
  RUN_TEST(test_short_press_down_up_tap);
  RUN_TEST(test_long_press_no_tap);
  RUN_TEST(test_drag_no_tap);
  RUN_TEST(test_ring_overflow_drops_oldest);
  RUN_TEST(test_defaults_leave_taps_unchanged);
  RUN_TEST(test_min_press_ms_suppresses_short_tap_only);
  RUN_TEST(test_max_taps_per_second_rate_limit);
  RUN_TEST(test_cell_corners);
  RUN_TEST(test_cell_light_and_outside);
  RUN_TEST(test_cell_rect_roundtrip);
  return UNITY_END();
}
