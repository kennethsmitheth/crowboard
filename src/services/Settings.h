#pragma once
// User settings persisted through IStorage as one versioned blob:
//   namespace "settings", key "v1", blob magic 'SET1', blob version 1.
// Payload layout (version 1, 13 bytes, field order fixed, no padding):
//   brightness(1) forcedCapture(1) theme(1) firstPlayer(1) defaultDifficulty(1)
//   p1Initials(4) p2Initials(4)
// Changing the layout requires a new blob version. Pure: no Arduino or ESP-IDF includes.
#include <stddef.h>
#include <stdint.h>

#include "IStorage.h"

#pragma pack(push, 1)
struct SettingsData {
  uint8_t brightness;         // percent, 10..100 (default 80)
  bool forcedCapture;         // default true
  uint8_t theme;              // theme id, default 0
  uint8_t firstPlayer;        // 0 = Player 1 (dark) moves first (default), 1 = Player 2
  uint8_t defaultDifficulty;  // 0 Easy, 1 Medium (default), 2 Hard
  char p1Initials[4];         // 3 printable ASCII chars + NUL (default "P1 ")
  char p2Initials[4];         // default "P2 "
};
#pragma pack(pop)
static_assert(sizeof(SettingsData) == 13, "SettingsData size changed: bump the blob version");

class Settings {
public:
  static constexpr const char* kNamespace = "settings";
  static constexpr const char* kKey = "v1";
  static constexpr uint32_t kMagic = 0x53455431u;  // "SET1"
  static constexpr uint16_t kVersion = 1;

  static constexpr uint8_t kMinBrightness = 10;
  static constexpr uint8_t kMaxBrightness = 100;
  static constexpr uint8_t kDefaultBrightness = 80;
  static constexpr uint8_t kMaxDifficulty = 2;
  static constexpr uint8_t kDefaultDifficulty = 1;

  // Default values for every field.
  static SettingsData defaults();

  explicit Settings(IStorage& storage);

  // Reads the blob into data(). On a missing, corrupt or wrong-version blob, data() becomes the
  // defaults and false is returned. Out-of-range values in a valid blob are clamped to their
  // defaults and still count as success.
  bool load();

  // Writes data() as-is (no clamping) and commits. Returns false on storage failure.
  bool save();

  SettingsData& data() { return data_; }
  const SettingsData& data() const { return data_; }

  // Replaces data() with the defaults. Does not touch storage.
  void resetToDefaults();

private:
  IStorage& storage_;
  SettingsData data_;
};
