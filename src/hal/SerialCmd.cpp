#include "SerialCmd.h"

#include "Log.h"

namespace {

// Run-length encodes a pixel stream; with emit == false it only counts the encoded bytes.
class RleWriter {
 public:
  explicit RleWriter(bool emit) : _emit(emit) {}

  void push(uint16_t px) {
    if (_run && px == _px && _run < 255) {
      _run++;
      return;
    }
    flush();
    _px = px;
    _run = 1;
  }

  void flush() {
    if (!_run) return;
    if (_emit) {
      const uint8_t tok[3] = {(uint8_t)(_px & 0xFF), (uint8_t)(_px >> 8), _run};
      Serial.write(tok, 3);
    }
    _bytes += 3;
    _run = 0;
  }

  size_t bytes() const { return _bytes; }

 private:
  bool _emit;
  uint16_t _px = 0;
  uint8_t _run = 0;
  size_t _bytes = 0;
};

}  // namespace

void SerialCmd::tick() {
  aiPoll();
  while (Serial.available()) {
    const char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (_len) {
        _line[_len] = '\0';
        _len = 0;
        handle(_line);
      }
    } else if (_len < kMaxLine - 1) {
      _line[_len++] = c;
    }
  }
}

void SerialCmd::handle(const char* line) {
  if (strcmp(line, "shot") == 0) {
    shot();
  } else if (strcmp(line, "heap") == 0) {
    _diag.report();
  } else if (strncmp(line, "ai", 2) == 0 && (line[2] == '\0' || line[2] == ' ')) {
    aiRequest(line[2] ? line + 3 : "");
  } else if (strcmp(line, "help") == 0) {
    Serial.println(
        "commands: shot (stream frame buffer), heap (diagnostics line), "
        "ai [easy|medium|hard] (search initial position), help");
    if (_handler) _handler(line, _ctx);
  } else if (!_handler || !_handler(line, _ctx)) {
    LOG_W("unknown command '%s' (try help)", line);
  }
}

void SerialCmd::shot() {
  const int32_t w = _gfx.width();
  const int32_t h = _gfx.height();
  static uint16_t row[800];
  if (w > 800) {
    LOG_E("shot: width %ld exceeds row buffer", (long)w);
    return;
  }

  // Pass 1: size the payload so the host can read an exact byte count.
  RleWriter sizer(false);
  for (int32_t y = 0; y < h; y++) {
    _gfx.readRect(0, y, w, 1, row);
    for (int32_t x = 0; x < w; x++) sizer.push(row[x]);
  }
  sizer.flush();

  g_logMuted = true;
  Serial.printf("SHOT %ld %ld RGB565RLE %u\n", (long)w, (long)h, (unsigned)sizer.bytes());
  RleWriter out(true);
  for (int32_t y = 0; y < h; y++) {
    _gfx.readRect(0, y, w, 1, row);
    for (int32_t x = 0; x < w; x++) out.push(row[x]);
  }
  out.flush();
  Serial.print("\nEND\n");
  Serial.flush();
  g_logMuted = false;
}

void SerialCmd::aiRequest(const char* arg) {
  if (!_ai) {
    LOG_W("ai: worker not attached");
    return;
  }
  checkers::Difficulty d = checkers::Difficulty::Medium;
  if (strcmp(arg, "easy") == 0) {
    d = checkers::Difficulty::Easy;
  } else if (strcmp(arg, "hard") == 0) {
    d = checkers::Difficulty::Hard;
  } else if (*arg && strcmp(arg, "medium") != 0) {
    LOG_W("ai: unknown difficulty '%s' (easy|medium|hard)", arg);
    return;
  }
  _aiId++;
  if (_ai->request(checkers::makeRequest(checkers::Board::initial(), d, _aiId))) {
    LOG_I("ai: request %u submitted", (unsigned)_aiId);
  } else {
    LOG_E("ai: request failed (worker not running)");
  }
}

void SerialCmd::aiPoll() {
  checkers::AiResult r;
  if (!_ai || !_ai->poll(r)) return;
  if (r.requestId != _aiId) {
    LOG_I("ai: dropped stale result %u", (unsigned)r.requestId);
    return;
  }
  LOG_I("ai: id %u move %u->%u depth %d score %d nodes %u %u ms completed %d, stack free %u B",
        (unsigned)r.requestId, r.move.from + 1, r.move.to + 1, r.depth, r.score, (unsigned)r.nodes,
        (unsigned)r.elapsedMs, (int)r.completed, (unsigned)_ai->stackHighWaterMark());
}
