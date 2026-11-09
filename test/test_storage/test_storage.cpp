// Storage and Blob tests (host). src/services must not include Arduino/ESP-IDF headers.
#include <string.h>
#include <unity.h>

#include "services/Blob.h"
#include "services/Crc32.h"
#include "services/RamStorage.h"

static const uint32_t kMagic = 0x43484B52;  // "CHKR"
static const char* kNs = "checkers";

static RamStorage ram;

void setUp(void) { ram.eraseNamespace(kNs); }
void tearDown(void) {}

static void test_crc32_known_vector(void) {
  TEST_ASSERT_EQUAL_HEX32(0xCBF43926u, crc32("123456789", 9));
}

static void test_ram_round_trip(void) {
  const uint8_t in[4] = {1, 2, 3, 4};
  uint8_t out[8] = {0};
  size_t len = 0;
  TEST_ASSERT_TRUE(ram.put(kNs, "k", in, 4));
  TEST_ASSERT_TRUE(ram.get(kNs, "k", out, sizeof(out), &len));
  TEST_ASSERT_EQUAL_UINT32(4, len);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(in, out, 4);
}

static void test_ram_overwrite(void) {
  uint8_t a[4] = {1, 2, 3, 4}, b[2] = {9, 8}, out[8];
  size_t len = 0;
  ram.put(kNs, "k", a, 4);
  ram.put(kNs, "k", b, 2);
  TEST_ASSERT_TRUE(ram.get(kNs, "k", out, sizeof(out), &len));
  TEST_ASSERT_EQUAL_UINT32(2, len);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(b, out, 2);
}

static void test_ram_erase(void) {
  uint8_t a = 7, out;
  ram.put(kNs, "k", &a, 1);
  ram.put(kNs, "j", &a, 1);
  TEST_ASSERT_TRUE(ram.erase(kNs, "k"));
  TEST_ASSERT_FALSE(ram.get(kNs, "k", &out, 1, nullptr));
  TEST_ASSERT_TRUE(ram.get(kNs, "j", &out, 1, nullptr));
  TEST_ASSERT_TRUE(ram.eraseNamespace(kNs));
  TEST_ASSERT_FALSE(ram.get(kNs, "j", &out, 1, nullptr));
}

static void test_ram_missing_and_limits(void) {
  uint8_t out[4] = {0};
  TEST_ASSERT_FALSE(ram.get(kNs, "nope", out, 4, nullptr));
  TEST_ASSERT_FALSE(ram.put(kNs, "0123456789abcdef", out, 1));  // key 16 chars
  TEST_ASSERT_FALSE(ram.put("0123456789abcdef", "k", out, 1));  // ns 16 chars
  TEST_ASSERT_FALSE(ram.put(kNs, "", out, 1));
  ram.put(kNs, "k", out, 4);
  TEST_ASSERT_FALSE(ram.get(kNs, "k", out, 2, nullptr));  // cap too small
}

struct Payload {
  uint32_t a;
  uint16_t b;
  char name[3];
};

static void test_blob_round_trip(void) {
  Payload in = {123456u, 42, {'A', 'B', 'C'}}, out;
  memset(&out, 0, sizeof(out));
  size_t len = 0;
  TEST_ASSERT_TRUE(Blob::save(ram, kNs, "p", kMagic, 1, &in, sizeof(in)));
  TEST_ASSERT_TRUE(Blob::load(ram, kNs, "p", kMagic, 1, &out, sizeof(out), &len));
  TEST_ASSERT_EQUAL_UINT32(sizeof(in), len);
  TEST_ASSERT_EQUAL_MEMORY(&in, &out, sizeof(in));
}

static void test_blob_missing(void) {
  Payload out;
  TEST_ASSERT_FALSE(Blob::load(ram, kNs, "p", kMagic, 1, &out, sizeof(out), nullptr));
}

static void saveSample(void) {
  Payload in = {1, 2, {'X', 'Y', 'Z'}};
  Blob::save(ram, kNs, "p", kMagic, 1, &in, sizeof(in));
}

static void test_blob_corrupt_payload(void) {
  saveSample();
  uint8_t* raw = ram.raw(kNs, "p");
  raw[Blob::kHeaderSize] ^= 0x01;
  Payload out;
  TEST_ASSERT_EQUAL(Blob::Status::BadCrc,
                    Blob::loadEx(ram, kNs, "p", kMagic, 1, &out, sizeof(out), nullptr));
}

static void test_blob_wrong_magic(void) {
  saveSample();
  Payload out;
  TEST_ASSERT_FALSE(Blob::load(ram, kNs, "p", kMagic + 1, 1, &out, sizeof(out), nullptr));
}

static void test_blob_wrong_version(void) {
  saveSample();
  Payload out;
  TEST_ASSERT_FALSE(Blob::load(ram, kNs, "p", kMagic, 2, &out, sizeof(out), nullptr));
}

static void test_blob_truncated(void) {
  saveSample();
  size_t total = 0;
  ram.raw(kNs, "p", &total);
  Payload out;
  TEST_ASSERT_TRUE(ram.truncate(kNs, "p", total - 1));
  TEST_ASSERT_EQUAL(Blob::Status::BadLength,
                    Blob::loadEx(ram, kNs, "p", kMagic, 1, &out, sizeof(out), nullptr));
  TEST_ASSERT_TRUE(ram.truncate(kNs, "p", 5));  // shorter than header
  TEST_ASSERT_FALSE(Blob::load(ram, kNs, "p", kMagic, 1, &out, sizeof(out), nullptr));
}

static void test_blob_cap_too_small(void) {
  saveSample();
  uint8_t out[2];
  TEST_ASSERT_FALSE(Blob::load(ram, kNs, "p", kMagic, 1, out, sizeof(out), nullptr));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_crc32_known_vector);
  RUN_TEST(test_ram_round_trip);
  RUN_TEST(test_ram_overwrite);
  RUN_TEST(test_ram_erase);
  RUN_TEST(test_ram_missing_and_limits);
  RUN_TEST(test_blob_round_trip);
  RUN_TEST(test_blob_missing);
  RUN_TEST(test_blob_corrupt_payload);
  RUN_TEST(test_blob_wrong_magic);
  RUN_TEST(test_blob_wrong_version);
  RUN_TEST(test_blob_truncated);
  RUN_TEST(test_blob_cap_too_small);
  return UNITY_END();
}
