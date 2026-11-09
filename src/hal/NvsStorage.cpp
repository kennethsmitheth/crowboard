#include "NvsStorage.h"

#include <nvs.h>
#include <nvs_flash.h>
#include <string.h>

static bool validName(const char* s) {
  if (!s) return false;
  size_t n = strlen(s);
  return n > 0 && n <= IStorage::kMaxNameLen;
}

bool NvsStorage::begin() {
  if (ready_) return true;
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    if (nvs_flash_erase() != ESP_OK) return false;
    err = nvs_flash_init();
  }
  ready_ = (err == ESP_OK);
  return ready_;
}

bool NvsStorage::get(const char* ns, const char* key, void* out, size_t cap, size_t* outLen) {
  if (!ready_ || !validName(ns) || !validName(key)) return false;
  nvs_handle_t h;
  if (nvs_open(ns, NVS_READONLY, &h) != ESP_OK) return false;
  size_t len = 0;
  bool ok = nvs_get_blob(h, key, nullptr, &len) == ESP_OK && len <= cap;
  if (ok && len) {
    size_t n = cap;
    ok = nvs_get_blob(h, key, out, &n) == ESP_OK;
    len = n;
  }
  nvs_close(h);
  if (ok && outLen) *outLen = len;
  return ok;
}

bool NvsStorage::put(const char* ns, const char* key, const void* data, size_t len) {
  if (!ready_ || !validName(ns) || !validName(key)) return false;
  nvs_handle_t h;
  if (nvs_open(ns, NVS_READWRITE, &h) != ESP_OK) return false;
  bool ok = nvs_set_blob(h, key, data, len) == ESP_OK && nvs_commit(h) == ESP_OK;
  nvs_close(h);
  return ok;
}

bool NvsStorage::erase(const char* ns, const char* key) {
  if (!ready_ || !validName(ns) || !validName(key)) return false;
  nvs_handle_t h;
  if (nvs_open(ns, NVS_READWRITE, &h) != ESP_OK) return false;
  esp_err_t err = nvs_erase_key(h, key);
  bool ok = (err == ESP_OK || err == ESP_ERR_NVS_NOT_FOUND) && nvs_commit(h) == ESP_OK;
  nvs_close(h);
  return ok;
}

bool NvsStorage::eraseNamespace(const char* ns) {
  if (!ready_ || !validName(ns)) return false;
  nvs_handle_t h;
  if (nvs_open(ns, NVS_READWRITE, &h) != ESP_OK) return false;
  bool ok = nvs_erase_all(h) == ESP_OK && nvs_commit(h) == ESP_OK;
  nvs_close(h);
  return ok;
}

bool NvsStorage::commit() {
  return ready_;
}
