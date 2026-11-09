#pragma once
// Namespaced blob storage. Pure interface: no Arduino or ESP-IDF includes.
// Implementations: hal/NvsStorage (device), services/RamStorage (tests).
//
// Limits (imposed by NVS, enforced by every implementation): namespace and key names must be
// non-empty and at most kMaxNameLen (15) characters. Violations make the call return false.
#include <stddef.h>

class IStorage {
 public:
  static constexpr size_t kMaxNameLen = 15;

  virtual ~IStorage() {}

  // Copies the blob into out (at most cap bytes). Returns false if the key is missing, the names
  // are invalid, or cap is too small. outLen (optional) receives the stored length when found.
  virtual bool get(const char* ns, const char* key, void* out, size_t cap, size_t* outLen) = 0;
  // Creates or overwrites a blob.
  virtual bool put(const char* ns, const char* key, const void* data, size_t len) = 0;
  // Removes one key. Returns true if the key no longer exists (including "was absent").
  virtual bool erase(const char* ns, const char* key) = 0;
  // Removes every key in a namespace.
  virtual bool eraseNamespace(const char* ns) = 0;
  // Flushes pending writes to persistent media.
  virtual bool commit() = 0;
};
