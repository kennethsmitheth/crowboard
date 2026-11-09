// Host tests for ui/WidgetCore: hit-testing, group dispatch (topmost, modal capture, visibility),
// press tracking, dirty collection and the initials buffer.
#include <unity.h>

#include "ui/WidgetCore.h"

using namespace checkers::ui;

void setUp(void) {}
void tearDown(void) {}

namespace {

struct TestWidget : CoreWidget {
  int downs = 0, moves = 0, ups = 0, taps = 0;
  bool consume = true;
  bool modal = false;
  TestWidget(int16_t x, int16_t y, int16_t w, int16_t h) { bounds = Rect{x, y, w, h}; }
  bool handle(const TouchEvent& e) override {
    switch (e.type) {
      case TouchType::Down:
        ++downs;
        break;
      case TouchType::Move:
        ++moves;
        break;
      case TouchType::Up:
        ++ups;
        break;
      case TouchType::Tap:
        ++taps;
        break;
    }
    return consume;
  }
  bool isModal() const override { return modal; }
};

using Group = GroupCore<CoreWidget, 16>;

TouchEvent ev(TouchType t, int x, int y) {
  return TouchEvent{t, static_cast<int16_t>(x), static_cast<int16_t>(y), 0};
}

}  // namespace

static void test_hit_test_edges(void) {
  const Rect r{10, 20, 30, 40};
  TEST_ASSERT_TRUE(hitTest(r, 10, 20));
  TEST_ASSERT_TRUE(hitTest(r, 39, 59));
  TEST_ASSERT_FALSE(hitTest(r, 40, 30));
  TEST_ASSERT_FALSE(hitTest(r, 20, 60));
  TEST_ASSERT_FALSE(hitTest(r, 9, 30));
}

static void test_dispatch_topmost_wins(void) {
  TestWidget below(0, 0, 100, 100), above(50, 50, 100, 100);
  Group g;
  g.add(&below);
  g.add(&above);
  TEST_ASSERT_TRUE(g.dispatch(ev(TouchType::Down, 60, 60)));  // overlap: above is on top
  TEST_ASSERT_EQUAL_INT(1, above.downs);
  TEST_ASSERT_EQUAL_INT(0, below.downs);
  TEST_ASSERT_TRUE(g.dispatch(ev(TouchType::Down, 10, 10)));  // only below contains it
  TEST_ASSERT_EQUAL_INT(1, below.downs);
  TEST_ASSERT_FALSE(g.dispatch(ev(TouchType::Down, 300, 300)));
}

static void test_non_consuming_falls_through(void) {
  TestWidget below(0, 0, 100, 100), above(0, 0, 100, 100);
  above.consume = false;
  Group g;
  g.add(&below);
  g.add(&above);
  TEST_ASSERT_TRUE(g.dispatch(ev(TouchType::Down, 5, 5)));
  TEST_ASSERT_EQUAL_INT(1, above.downs);
  TEST_ASSERT_EQUAL_INT(1, below.downs);
}

static void test_move_and_up_follow_the_down_target(void) {
  TestWidget a(0, 0, 100, 100), b(200, 0, 100, 100);
  Group g;
  g.add(&a);
  g.add(&b);
  g.dispatch(ev(TouchType::Down, 10, 10));
  g.dispatch(ev(TouchType::Move, 250, 10));  // finger wandered over b
  g.dispatch(ev(TouchType::Up, 250, 10));
  TEST_ASSERT_EQUAL_INT(1, a.moves);
  TEST_ASSERT_EQUAL_INT(1, a.ups);
  TEST_ASSERT_EQUAL_INT(0, b.moves);
  TEST_ASSERT_EQUAL_INT(0, b.ups);
  TEST_ASSERT_FALSE(g.dispatch(ev(TouchType::Up, 250, 10)));  // nothing active any more
}

static void test_tap_after_up_is_not_rerouted(void) {
  TestWidget a(0, 0, 100, 100), b(0, 0, 100, 100);
  Group g;
  g.add(&b);
  g.add(&a);
  g.dispatch(ev(TouchType::Down, 10, 10));
  a.visible = false;  // e.g. a keypad that closed itself on Up
  g.dispatch(ev(TouchType::Up, 10, 10));
  g.dispatch(ev(TouchType::Tap, 10, 10));
  TEST_ASSERT_EQUAL_INT(0, b.taps);
  TEST_ASSERT_EQUAL_INT(0, b.downs);
}

static void test_modal_captures_everything(void) {
  TestWidget under(0, 0, 800, 480), modal(200, 100, 400, 200);
  modal.modal = true;
  Group g;
  g.add(&under);
  g.add(&modal);
  TEST_ASSERT_TRUE(g.dispatch(ev(TouchType::Down, 10, 10)));  // outside the modal panel
  TEST_ASSERT_EQUAL_INT(1, modal.downs);
  TEST_ASSERT_EQUAL_INT(0, under.downs);
  g.dispatch(ev(TouchType::Up, 10, 10));
  g.dispatch(ev(TouchType::Tap, 10, 10));
  TEST_ASSERT_EQUAL_INT(0, under.taps);
  modal.consume = false;  // even if it does not consume, the group reports it swallowed
  TEST_ASSERT_TRUE(g.dispatch(ev(TouchType::Down, 300, 150)));
  TEST_ASSERT_EQUAL_INT(0, under.downs);
}

static void test_hidden_modal_does_not_capture(void) {
  TestWidget under(0, 0, 800, 480), modal(200, 100, 400, 200);
  modal.modal = true;
  modal.visible = false;
  Group g;
  g.add(&under);
  g.add(&modal);
  g.dispatch(ev(TouchType::Down, 300, 150));
  TEST_ASSERT_EQUAL_INT(1, under.downs);
  TEST_ASSERT_EQUAL_INT(0, modal.downs);
}

static void test_invisible_widgets_are_skipped(void) {
  TestWidget below(0, 0, 100, 100), above(0, 0, 100, 100);
  above.visible = false;
  Group g;
  g.add(&below);
  g.add(&above);
  g.dispatch(ev(TouchType::Down, 5, 5));
  TEST_ASSERT_EQUAL_INT(0, above.downs);
  TEST_ASSERT_EQUAL_INT(1, below.downs);
}

static void test_group_capacity(void) {
  TestWidget w(0, 0, 1, 1);
  Group g;
  for (int i = 0; i < 16; ++i) TEST_ASSERT_TRUE(g.add(&w));
  TEST_ASSERT_FALSE(g.add(&w));
  TEST_ASSERT_FALSE(g.add(nullptr));
  TEST_ASSERT_EQUAL_INT(16, g.count());
}

static void test_collect_dirty_marks_exactly_dirty_widgets(void) {
  TestWidget a(0, 0, 100, 50), b(200, 100, 80, 60), c(400, 300, 50, 50);
  a.dirty = false;
  b.dirty = true;
  c.dirty = false;
  Group g;
  g.add(&a);
  g.add(&b);
  g.add(&c);
  DirtyRects d;
  TEST_ASSERT_EQUAL_INT(1, g.collectDirty(d));
  TEST_ASSERT_EQUAL_INT(1, d.rectCount());
  TEST_ASSERT_TRUE((Rect{200, 100, 80, 60}) == d.rect(0));
  TEST_ASSERT_EQUAL_UINT64(0, d.cells());
  TEST_ASSERT_FALSE(a.dirty);
  TEST_ASSERT_FALSE(b.dirty);
  TEST_ASSERT_FALSE(c.dirty);
  DirtyRects d2;
  TEST_ASSERT_EQUAL_INT(0, g.collectDirty(d2));  // flags were cleared
  TEST_ASSERT_FALSE(d2.any());
}

static void test_partial_invalidate_unions(void) {
  TestWidget a(100, 100, 300, 200);
  a.dirty = false;
  a.invalidate(Rect{100, 100, 10, 10});
  a.invalidate(Rect{150, 150, 10, 10});
  Group g;
  g.add(&a);
  DirtyRects d;
  g.collectDirty(d);
  TEST_ASSERT_TRUE((Rect{100, 100, 60, 60}) == d.rect(0));
  a.invalidate();  // whole bounds
  g.collectDirty(d);
  TEST_ASSERT_TRUE(d.rect(0) == (Rect{100, 100, 300, 200}));
}

static void test_hiding_marks_the_old_area(void) {
  TestWidget a(10, 10, 100, 100);
  a.dirty = false;
  Group g;
  g.add(&a);
  a.setVisible(false);
  DirtyRects d;
  TEST_ASSERT_EQUAL_INT(1, g.collectDirty(d));  // hidden widgets are still collected
  TEST_ASSERT_TRUE((Rect{10, 10, 100, 100}) == d.rect(0));
  a.setVisible(false);  // no change, no new mark
  TEST_ASSERT_EQUAL_INT(0, g.collectDirty(d));
}

static void test_set_bounds_marks_old_and_new(void) {
  TestWidget a(0, 0, 50, 50);
  a.dirty = false;
  a.setBounds(Rect{100, 0, 50, 50});
  Group g;
  g.add(&a);
  DirtyRects d;
  g.collectDirty(d);
  TEST_ASSERT_TRUE(d.rect(0) == (Rect{100, 0, 50, 50}));  // dirtyRect is the new bounds
}

static void test_region_press_fires_on_up_inside(void) {
  RegionPress p;
  TEST_ASSERT_TRUE(p.down(3));
  TEST_ASSERT_EQUAL_INT(3, p.active());
  TEST_ASSERT_TRUE(p.move(-1));
  TEST_ASSERT_EQUAL_INT(-1, p.active());
  TEST_ASSERT_TRUE(p.move(3));
  TEST_ASSERT_EQUAL_INT(3, p.up(3));
  TEST_ASSERT_EQUAL_INT(-1, p.active());
  TEST_ASSERT_EQUAL_INT(-1, p.tap(3));  // the Tap after a firing Up must not fire again
  TEST_ASSERT_EQUAL_INT(3, p.tap(3));   // a lone Tap fires
}

static void test_region_press_up_elsewhere_cancels(void) {
  RegionPress p;
  p.down(1);
  TEST_ASSERT_EQUAL_INT(-1, p.up(2));
  TEST_ASSERT_EQUAL_INT(-1, p.up(1));  // nothing pressed any more
}

static void test_keypad_text_logic(void) {
  InitialsBuffer b;
  TEST_ASSERT_EQUAL_STRING("", b.text());
  TEST_ASSERT_FALSE(b.backspace());
  TEST_ASSERT_TRUE(b.append('K'));
  TEST_ASSERT_TRUE(b.append('m'));  // lower case is upper-cased
  TEST_ASSERT_TRUE(b.append('S'));
  TEST_ASSERT_TRUE(b.full());
  TEST_ASSERT_FALSE(b.append('Z'));  // capped at 3
  TEST_ASSERT_FALSE(b.append('1'));
  TEST_ASSERT_EQUAL_STRING("KMS", b.text());
  TEST_ASSERT_TRUE(b.backspace());
  TEST_ASSERT_EQUAL_STRING("KM", b.text());
  TEST_ASSERT_EQUAL_INT(2, b.length());
  b.clear();
  TEST_ASSERT_EQUAL_INT(0, b.length());
  TEST_ASSERT_EQUAL_STRING("", b.text());
}

static void test_copy_text_truncates(void) {
  char buf[5];
  copyText(buf, sizeof buf, "abcdefgh");
  TEST_ASSERT_EQUAL_STRING("abcd", buf);
  copyText(buf, sizeof buf, nullptr);
  TEST_ASSERT_EQUAL_STRING("", buf);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_hit_test_edges);
  RUN_TEST(test_dispatch_topmost_wins);
  RUN_TEST(test_non_consuming_falls_through);
  RUN_TEST(test_move_and_up_follow_the_down_target);
  RUN_TEST(test_tap_after_up_is_not_rerouted);
  RUN_TEST(test_modal_captures_everything);
  RUN_TEST(test_hidden_modal_does_not_capture);
  RUN_TEST(test_invisible_widgets_are_skipped);
  RUN_TEST(test_group_capacity);
  RUN_TEST(test_collect_dirty_marks_exactly_dirty_widgets);
  RUN_TEST(test_partial_invalidate_unions);
  RUN_TEST(test_hiding_marks_the_old_area);
  RUN_TEST(test_set_bounds_marks_old_and_new);
  RUN_TEST(test_region_press_fires_on_up_inside);
  RUN_TEST(test_region_press_up_elsewhere_cancels);
  RUN_TEST(test_keypad_text_logic);
  RUN_TEST(test_copy_text_truncates);
  return UNITY_END();
}
