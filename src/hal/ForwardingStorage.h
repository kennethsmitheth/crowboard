// IStorage that delegates to a swappable target. No Arduino or ESP-IDF includes (host-testable).
//
// Services take an IStorage& at static-init time, before it is known whether NVS works. They are
// given this object; StorageSelector points it at NVS or RAM once begin() has run. With no target
// every call fails (returns false), which services treat as "nothing stored".
#pragma once

#include "services/IStorage.h"

class ForwardingStorage : public IStorage {
 public:
  void setTarget(IStorage* target) { target_ = target; }
  IStorage* target() const { return target_; }

  bool get(const char* ns, const char* key, void* out, size_t cap, size_t* outLen) override {
    return target_ && target_->get(ns, key, out, cap, outLen);
  }
  bool put(const char* ns, const char* key, const void* data, size_t len) override {
    return target_ && target_->put(ns, key, data, len);
  }
  bool erase(const char* ns, const char* key) override {
    return target_ && target_->erase(ns, key);
  }
  bool eraseNamespace(const char* ns) override { return target_ && target_->eraseNamespace(ns); }
  bool commit() override { return target_ && target_->commit(); }

 private:
  IStorage* target_ = nullptr;
};
