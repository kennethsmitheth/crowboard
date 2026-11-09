#include "SaveGame.h"

#include <string.h>

namespace checkers {

namespace {

void put32(uint8_t* p, uint32_t v) {
  for (int i = 0; i < 4; ++i) p[i] = static_cast<uint8_t>(v >> (8 * i));
}
uint32_t get32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | static_cast<uint32_t>(p[1]) << 8 |
         static_cast<uint32_t>(p[2]) << 16 | static_cast<uint32_t>(p[3]) << 24;
}

}  // namespace

bool SaveGame::exists() {
  uint8_t buf[Blob::kMaxPayload];
  size_t len = 0;
  return Blob::load(storage_, kNamespace, kKey, kMagic, kVersion, buf, sizeof(buf), &len) &&
         len >= kHeaderBytes;
}

bool SaveGame::save(const Game& game, const SaveHeader& header) {
  uint8_t buf[Blob::kMaxPayload];
  const uint16_t n = game.historySize();
  buf[0] = header.mode;
  buf[1] = header.difficulty;
  buf[2] = header.humanSide;
  buf[3] = game.rules().forcedCapture ? 1 : 0;
  put32(buf + 4, header.p1ClockSec);
  put32(buf + 8, header.p2ClockSec);
  buf[12] = static_cast<uint8_t>(n);
  buf[13] = static_cast<uint8_t>(n >> 8);
  size_t pos = kHeaderBytes;
  for (uint16_t i = 0; i < n; ++i) {
    const Move& m = game.historyAt(i);
    if (m.pathLen == 0 || m.pathLen > kMaxPathLen) return false;
    if (pos + 2 + m.pathLen > sizeof(buf)) return false;  // over budget: write nothing
    buf[pos++] = m.from;
    buf[pos++] = m.pathLen;
    for (uint8_t k = 0; k < m.pathLen; ++k) buf[pos++] = m.path[k];
  }
  if (!Blob::save(storage_, kNamespace, kKey, kMagic, kVersion, buf, pos)) return false;
  return storage_.commit();
}

bool SaveGame::load(Game& outGame, SaveHeader& outHeader) {
  uint8_t buf[Blob::kMaxPayload];
  size_t len = 0;
  Rules rules;
  rules.forcedCapture = true;
  Blob::Status st =
      Blob::loadEx(storage_, kNamespace, kKey, kMagic, kVersion, buf, sizeof(buf), &len);
  if (st != Blob::Status::Ok || len < kHeaderBytes) {
    outGame.reset(rules);
    return false;
  }
  SaveHeader h;
  h.mode = buf[0];
  h.difficulty = buf[1];
  h.humanSide = buf[2];
  h.forcedCapture = buf[3];
  h.p1ClockSec = get32(buf + 4);
  h.p2ClockSec = get32(buf + 8);
  h.moveCount = static_cast<uint16_t>(buf[12] | buf[13] << 8);
  if (h.mode > 1 || h.difficulty >= kDifficultyCount || h.humanSide > 1 || h.forcedCapture > 1 ||
      h.moveCount > kMaxPlies) {
    outGame.reset(rules);
    return false;
  }
  rules.forcedCapture = h.forcedCapture != 0;
  outGame.reset(rules);

  size_t pos = kHeaderBytes;
  for (uint16_t i = 0; i < h.moveCount; ++i) {
    if (pos + 2 > len) {
      outGame.reset(rules);
      return false;
    }
    const uint8_t from = buf[pos++];
    const uint8_t pathLen = buf[pos++];
    if (pathLen == 0 || pathLen > kMaxPathLen || pos + pathLen > len) {
      outGame.reset(rules);
      return false;
    }
    const uint8_t* path = buf + pos;
    pos += pathLen;

    bool ok = false;
    const MoveList& legal = outGame.legalMoves();
    for (size_t j = 0; j < legal.size(); ++j) {
      const Move& m = legal[j];
      if (m.from != from || m.pathLen != pathLen || memcmp(m.path, path, pathLen) != 0) continue;
      const Move chosen = m;  // copy: apply() invalidates the cached list
      ok = outGame.apply(chosen);
      break;
    }
    if (!ok) {
      outGame.reset(rules);
      return false;
    }
  }
  if (pos != len) {  // trailing garbage
    outGame.reset(rules);
    return false;
  }
  outHeader = h;
  return true;
}

void SaveGame::erase() {
  storage_.erase(kNamespace, kKey);
  storage_.commit();
}

}  // namespace checkers
