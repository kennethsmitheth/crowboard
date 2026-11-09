#pragma once
// Versioned, CRC-checked blobs on top of IStorage.
// Stored layout: Header (12 bytes, little-endian) followed by the payload:
//   {uint32_t magic; uint16_t version; uint16_t length; uint32_t crc32;}
// load() returns false ("absent") on a missing key, magic/version mismatch, length mismatch or
// bad CRC. Callers fall back to defaults. Payloads are limited to kMaxPayload bytes.
#include <stddef.h>
#include <stdint.h>

#include "IStorage.h"

namespace Blob {

constexpr size_t kHeaderSize = 12;
constexpr size_t kMaxPayload = 1024;

enum class Status : uint8_t { Ok, Missing, BadHeader, BadLength, BadCrc };

bool save(IStorage& s, const char* ns, const char* key, uint32_t magic, uint16_t version,
          const void* data, size_t len);

bool load(IStorage& s, const char* ns, const char* key, uint32_t magic, uint16_t version,
          void* out, size_t cap, size_t* outLen);

// Same as load() but reports why it failed (for logging by the caller).
Status loadEx(IStorage& s, const char* ns, const char* key, uint32_t magic, uint16_t version,
              void* out, size_t cap, size_t* outLen);

}  // namespace Blob
