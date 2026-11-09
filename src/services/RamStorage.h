#pragma once
// Fixed-capacity in-memory IStorage for tests. No heap.
#include <stdint.h>
#include <string.h>

#include "IStorage.h"

template <size_t kEntries = 16, size_t kMaxBlob = 1100>
class RamStorageT : public IStorage {
 public:
  bool get(const char* ns, const char* key, void* out, size_t cap, size_t* outLen) override {
    Entry* e = find(ns, key);
    if (!e || e->len > cap) return false;
    if (e->len) memcpy(out, e->data, e->len);
    if (outLen) *outLen = e->len;
    return true;
  }

  bool put(const char* ns, const char* key, const void* data, size_t len) override {
    if (!validName(ns) || !validName(key) || len > kMaxBlob) return false;
    Entry* e = find(ns, key);
    if (!e) {
      for (size_t i = 0; i < kEntries && !e; ++i) {
        if (!entries_[i].used) e = &entries_[i];
      }
      if (!e) return false;  // full
      e->used = true;
      strcpy(e->ns, ns);
      strcpy(e->key, key);
    }
    if (len) memcpy(e->data, data, len);
    e->len = len;
    return true;
  }

  bool erase(const char* ns, const char* key) override {
    if (!validName(ns) || !validName(key)) return false;
    Entry* e = find(ns, key);
    if (e) e->used = false;
    return true;
  }

  bool eraseNamespace(const char* ns) override {
    if (!validName(ns)) return false;
    for (size_t i = 0; i < kEntries; ++i) {
      if (entries_[i].used && strcmp(entries_[i].ns, ns) == 0) entries_[i].used = false;
    }
    return true;
  }

  bool commit() override { return true; }

  // Test helper: direct access to stored bytes (e.g. to corrupt them). Null if absent.
  uint8_t* raw(const char* ns, const char* key, size_t* len = nullptr) {
    Entry* e = find(ns, key);
    if (!e) return nullptr;
    if (len) *len = e->len;
    return e->data;
  }

  // Test helper: shrink a stored blob to simulate truncation.
  bool truncate(const char* ns, const char* key, size_t newLen) {
    Entry* e = find(ns, key);
    if (!e || newLen > e->len) return false;
    e->len = newLen;
    return true;
  }

 private:
  struct Entry {
    bool used = false;
    char ns[kMaxNameLen + 1] = {0};
    char key[kMaxNameLen + 1] = {0};
    size_t len = 0;
    uint8_t data[kMaxBlob];
  };

  static bool validName(const char* s) {
    if (!s) return false;
    size_t n = strlen(s);
    return n > 0 && n <= kMaxNameLen;
  }

  Entry* find(const char* ns, const char* key) {
    if (!validName(ns) || !validName(key)) return nullptr;
    for (size_t i = 0; i < kEntries; ++i) {
      Entry& e = entries_[i];
      if (e.used && strcmp(e.ns, ns) == 0 && strcmp(e.key, key) == 0) return &e;
    }
    return nullptr;
  }

  Entry entries_[kEntries];
};

using RamStorage = RamStorageT<>;
