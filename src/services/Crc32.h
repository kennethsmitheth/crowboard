#pragma once
// Table-less bitwise CRC-32 (IEEE 802.3, reflected, poly 0xEDB88320).
#include <stddef.h>
#include <stdint.h>

inline uint32_t crc32(const void* data, size_t len) {
  const uint8_t* p = static_cast<const uint8_t*>(data);
  uint32_t crc = 0xFFFFFFFFu;
  for (size_t i = 0; i < len; ++i) {
    crc ^= p[i];
    for (int b = 0; b < 8; ++b) {
      crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
  }
  return ~crc;
}
