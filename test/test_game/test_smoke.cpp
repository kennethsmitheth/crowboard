// Smoke test for the native environment.
// src/game and src/services must never include Arduino or ESP-IDF headers.
#include <unity.h>

void setUp(void) {}
void tearDown(void) {}

static void test_smoke(void) {
  TEST_ASSERT_EQUAL_INT(4, 2 + 2);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_smoke);
  return UNITY_END();
}
