// Settings service tests (host). Uses RamStorage; no Arduino or ESP-IDF headers.
#include <string.h>
#include <unity.h>

#include "services/Blob.h"
#include "services/RamStorage.h"
#include "services/Settings.h"

static RamStorage ram;

void setUp(void) {
  ram.eraseNamespace(Settings::kNamespace);
}
void tearDown(void) {}

static void assertDefaults(const SettingsData& d) {
  TEST_ASSERT_EQUAL_UINT8(80, d.brightness);
  TEST_ASSERT_TRUE(d.forcedCapture);
  TEST_ASSERT_EQUAL_UINT8(0, d.theme);
  TEST_ASSERT_EQUAL_UINT8(0, d.firstPlayer);
  TEST_ASSERT_EQUAL_UINT8(1, d.defaultDifficulty);
  TEST_ASSERT_EQUAL_STRING("P1 ", d.p1Initials);
  TEST_ASSERT_EQUAL_STRING("P2 ", d.p2Initials);
}

static void test_defaults_when_empty(void) {
  Settings s(ram);
  TEST_ASSERT_FALSE(s.load());
  assertDefaults(s.data());
}

static void test_round_trip_all_fields(void) {
  Settings out(ram);
  out.data().brightness = 42;
  out.data().forcedCapture = false;
  out.data().theme = 3;
  out.data().firstPlayer = 1;
  out.data().defaultDifficulty = 2;
  strcpy(out.data().p1Initials, "ABC");
  strcpy(out.data().p2Initials, "X1 ");
  TEST_ASSERT_TRUE(out.save());

  Settings in(ram);
  TEST_ASSERT_TRUE(in.load());
  const SettingsData& d = in.data();
  TEST_ASSERT_EQUAL_UINT8(42, d.brightness);
  TEST_ASSERT_FALSE(d.forcedCapture);
  TEST_ASSERT_EQUAL_UINT8(3, d.theme);
  TEST_ASSERT_EQUAL_UINT8(1, d.firstPlayer);
  TEST_ASSERT_EQUAL_UINT8(2, d.defaultDifficulty);
  TEST_ASSERT_EQUAL_STRING("ABC", d.p1Initials);
  TEST_ASSERT_EQUAL_STRING("X1 ", d.p2Initials);
}

static void test_corrupt_payload_yields_defaults(void) {
  Settings out(ram);
  out.data().brightness = 42;
  TEST_ASSERT_TRUE(out.save());
  uint8_t* raw = ram.raw(Settings::kNamespace, Settings::kKey);
  TEST_ASSERT_NOT_NULL(raw);
  raw[Blob::kHeaderSize] ^= 0x01;  // breaks the CRC

  Settings in(ram);
  in.data().brightness = 1;  // must be overwritten by defaults on failure
  TEST_ASSERT_FALSE(in.load());
  assertDefaults(in.data());
}

static void test_truncated_blob_yields_defaults(void) {
  Settings out(ram);
  TEST_ASSERT_TRUE(out.save());
  TEST_ASSERT_TRUE(ram.truncate(Settings::kNamespace, Settings::kKey, 3));

  Settings in(ram);
  TEST_ASSERT_FALSE(in.load());
  assertDefaults(in.data());
}

static void test_short_but_valid_blob_yields_defaults(void) {
  // A blob with a valid header and CRC but a shorter payload than version 1 must not be read.
  uint8_t shortPayload[4] = {1, 2, 3, 4};
  TEST_ASSERT_TRUE(Blob::save(ram, Settings::kNamespace, Settings::kKey, Settings::kMagic,
                              Settings::kVersion, shortPayload, sizeof(shortPayload)));
  Settings in(ram);
  TEST_ASSERT_FALSE(in.load());
  assertDefaults(in.data());
}

static void test_out_of_range_brightness_clamped(void) {
  Settings out(ram);
  out.data().brightness = 5;
  TEST_ASSERT_TRUE(out.save());
  Settings in(ram);
  TEST_ASSERT_TRUE(in.load());
  TEST_ASSERT_EQUAL_UINT8(80, in.data().brightness);

  out.data().brightness = 101;
  TEST_ASSERT_TRUE(out.save());
  TEST_ASSERT_TRUE(in.load());
  TEST_ASSERT_EQUAL_UINT8(80, in.data().brightness);

  out.data().brightness = 10;  // boundary values are kept
  TEST_ASSERT_TRUE(out.save());
  TEST_ASSERT_TRUE(in.load());
  TEST_ASSERT_EQUAL_UINT8(10, in.data().brightness);
}

static void test_out_of_range_fields_clamped(void) {
  Settings out(ram);
  out.data().defaultDifficulty = 7;
  out.data().firstPlayer = 9;
  out.data().p1Initials[0] = '\x01';
  out.data().p2Initials[3] = 'Z';  // not NUL-terminated
  TEST_ASSERT_TRUE(out.save());

  Settings in(ram);
  TEST_ASSERT_TRUE(in.load());
  TEST_ASSERT_EQUAL_UINT8(1, in.data().defaultDifficulty);
  TEST_ASSERT_EQUAL_UINT8(0, in.data().firstPlayer);
  TEST_ASSERT_EQUAL_STRING("P1 ", in.data().p1Initials);
  TEST_ASSERT_EQUAL_STRING("P2 ", in.data().p2Initials);
}

static void test_reset_then_save_then_load_gives_defaults(void) {
  Settings s(ram);
  s.data().brightness = 30;
  s.data().forcedCapture = false;
  s.data().defaultDifficulty = 2;
  TEST_ASSERT_TRUE(s.save());

  s.resetToDefaults();
  assertDefaults(s.data());
  TEST_ASSERT_TRUE(s.save());

  Settings in(ram);
  TEST_ASSERT_TRUE(in.load());
  assertDefaults(in.data());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_defaults_when_empty);
  RUN_TEST(test_round_trip_all_fields);
  RUN_TEST(test_corrupt_payload_yields_defaults);
  RUN_TEST(test_truncated_blob_yields_defaults);
  RUN_TEST(test_short_but_valid_blob_yields_defaults);
  RUN_TEST(test_out_of_range_brightness_clamped);
  RUN_TEST(test_out_of_range_fields_clamped);
  RUN_TEST(test_reset_then_save_then_load_gives_defaults);
  return UNITY_END();
}
