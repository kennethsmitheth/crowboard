#include "Settings.h"

#include <string.h>

#include "Blob.h"

namespace {

// Byte-typed mirror of the version 1 payload. Decoding through uint8_t avoids reading arbitrary
// bytes as bool, which is undefined behavior.
struct WireV1 {
  uint8_t brightness;
  uint8_t forcedCapture;
  uint8_t theme;
  uint8_t firstPlayer;
  uint8_t defaultDifficulty;
  char p1Initials[4];
  char p2Initials[4];
};
static_assert(sizeof(WireV1) == 13, "WireV1 must match the version 1 payload");

WireV1 toWire(const SettingsData& d) {
  WireV1 w;
  w.brightness = d.brightness;
  w.forcedCapture = d.forcedCapture ? 1 : 0;
  w.theme = d.theme;
  w.firstPlayer = d.firstPlayer;
  w.defaultDifficulty = d.defaultDifficulty;
  memcpy(w.p1Initials, d.p1Initials, sizeof(w.p1Initials));
  memcpy(w.p2Initials, d.p2Initials, sizeof(w.p2Initials));
  return w;
}

SettingsData fromWire(const WireV1& w) {
  SettingsData d;
  d.brightness = w.brightness;
  d.forcedCapture = w.forcedCapture != 0;
  d.theme = w.theme;
  d.firstPlayer = w.firstPlayer;
  d.defaultDifficulty = w.defaultDifficulty;
  memcpy(d.p1Initials, w.p1Initials, sizeof(d.p1Initials));
  memcpy(d.p2Initials, w.p2Initials, sizeof(d.p2Initials));
  return d;
}

// Valid initials: three printable ASCII characters followed by NUL.
bool validInitials(const char* s) {
  for (int i = 0; i < 3; ++i) {
    if (s[i] < 0x20 || s[i] > 0x7E) return false;
  }
  return s[3] == '\0';
}

void setInitials(char* dst, const char* value) {
  memcpy(dst, value, 4);
}

// Replaces out-of-range values with their defaults.
void clamp(SettingsData& d) {
  const SettingsData def = Settings::defaults();
  if (d.brightness < Settings::kMinBrightness || d.brightness > Settings::kMaxBrightness) {
    d.brightness = def.brightness;
  }
  if (d.firstPlayer > 1) d.firstPlayer = def.firstPlayer;
  if (d.defaultDifficulty > Settings::kMaxDifficulty) {
    d.defaultDifficulty = def.defaultDifficulty;
  }
  if (!validInitials(d.p1Initials)) setInitials(d.p1Initials, def.p1Initials);
  if (!validInitials(d.p2Initials)) setInitials(d.p2Initials, def.p2Initials);
}

}  // namespace

SettingsData Settings::defaults() {
  SettingsData d;
  d.brightness = kDefaultBrightness;
  d.forcedCapture = true;
  d.theme = 0;
  d.firstPlayer = 0;
  d.defaultDifficulty = kDefaultDifficulty;
  setInitials(d.p1Initials, "P1 ");
  setInitials(d.p2Initials, "P2 ");
  return d;
}

Settings::Settings(IStorage& storage) : storage_(storage), data_(defaults()) {}

bool Settings::load() {
  WireV1 w;
  size_t len = 0;
  Blob::Status st = Blob::loadEx(storage_, kNamespace, kKey, kMagic, kVersion, &w, sizeof(w), &len);
  if (st != Blob::Status::Ok || len != sizeof(w)) {
    data_ = defaults();
    return false;
  }
  data_ = fromWire(w);
  clamp(data_);
  return true;
}

bool Settings::save() {
  WireV1 w = toWire(data_);
  return Blob::save(storage_, kNamespace, kKey, kMagic, kVersion, &w, sizeof(w));
}

void Settings::resetToDefaults() {
  data_ = defaults();
}
