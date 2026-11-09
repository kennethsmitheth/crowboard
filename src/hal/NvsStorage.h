#pragma once
// IStorage on ESP-IDF NVS. Namespaces and keys are limited to 15 characters (enforced).
// Each put() commits immediately; commit() is kept for interface symmetry.
#include "services/IStorage.h"

class NvsStorage : public IStorage {
public:
  // Initialises the NVS partition once (erases and re-inits on "no free pages / new version").
  bool begin();

  bool get(const char* ns, const char* key, void* out, size_t cap, size_t* outLen) override;
  bool put(const char* ns, const char* key, const void* data, size_t len) override;
  bool erase(const char* ns, const char* key) override;
  bool eraseNamespace(const char* ns) override;
  bool commit() override;

private:
  bool ready_ = false;
};
