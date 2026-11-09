#include "Blob.h"

#include <string.h>

#include "Crc32.h"

namespace Blob {

static void put32(uint8_t* p, uint32_t v) {
  for (int i = 0; i < 4; ++i) p[i] = static_cast<uint8_t>(v >> (8 * i));
}
static void put16(uint8_t* p, uint16_t v) {
  p[0] = static_cast<uint8_t>(v);
  p[1] = static_cast<uint8_t>(v >> 8);
}
static uint32_t get32(const uint8_t* p) {
  return p[0] | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}
static uint16_t get16(const uint8_t* p) {
  return uint16_t(p[0] | (p[1] << 8));
}

bool save(IStorage& s, const char* ns, const char* key, uint32_t magic, uint16_t version,
          const void* data, size_t len) {
  if (len > kMaxPayload || (len && !data)) return false;
  uint8_t buf[kHeaderSize + kMaxPayload];
  put32(buf, magic);
  put16(buf + 4, version);
  put16(buf + 6, static_cast<uint16_t>(len));
  put32(buf + 8, crc32(data, len));
  if (len) memcpy(buf + kHeaderSize, data, len);
  return s.put(ns, key, buf, kHeaderSize + len) && s.commit();
}

Status loadEx(IStorage& s, const char* ns, const char* key, uint32_t magic, uint16_t version,
              void* out, size_t cap, size_t* outLen) {
  uint8_t buf[kHeaderSize + kMaxPayload];
  size_t total = 0;
  if (!s.get(ns, key, buf, sizeof(buf), &total)) return Status::Missing;
  if (total < kHeaderSize) return Status::BadHeader;
  if (get32(buf) != magic || get16(buf + 4) != version) return Status::BadHeader;
  size_t len = get16(buf + 6);
  if (len != total - kHeaderSize || len > cap) return Status::BadLength;
  if (crc32(buf + kHeaderSize, len) != get32(buf + 8)) return Status::BadCrc;
  if (len) memcpy(out, buf + kHeaderSize, len);
  if (outLen) *outLen = len;
  return Status::Ok;
}

bool load(IStorage& s, const char* ns, const char* key, uint32_t magic, uint16_t version, void* out,
          size_t cap, size_t* outLen) {
  return loadEx(s, ns, key, magic, version, out, cap, outLen) == Status::Ok;
}

}  // namespace Blob
