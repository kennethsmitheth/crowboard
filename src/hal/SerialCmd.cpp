#include "SerialCmd.h"

#include "Log.h"

namespace {

// aibench runs: each position at a fixed depth (no budget) without and with the TT, then under
// each difficulty profile. The mid-game position (8 v 8, Black to move) is ply 20 of a host
// self-play game from the initial position.
constexpr const char* kBenchMid = "B:W1,3,4,9,10,11,12,14:B18,21,23,24,29,30,31,32";
constexpr int kBenchFixedDepth = 6;
struct BenchRun {
  bool mid;
  const char* label;
  int profile;  // -1: fixed depth without TT, -2: fixed depth with TT, else Difficulty index
};
constexpr BenchRun kBenchRuns[] = {
    {false, "d6", -1},   {false, "d6+tt", -2}, {false, "easy", 0},  {false, "medium", 1},
    {false, "hard", 2},  {true, "d6", -1},     {true, "d6+tt", -2}, {true, "easy", 0},
    {true, "medium", 1}, {true, "hard", 2},
};
constexpr int kBenchCount = sizeof(kBenchRuns) / sizeof(kBenchRuns[0]);

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
  } else if (strncmp(line, "aibench", 7) == 0 && (line[7] == '\0' || line[7] == ' ')) {
    aiBenchStart(line[7] ? line + 8 : "");
  } else if (strncmp(line, "ai", 2) == 0 && (line[2] == '\0' || line[2] == ' ')) {
    aiRequest(line[2] ? line + 3 : "");
#ifdef CROWBOARD_DEBUG_CMDS
  } else if (strncmp(line, "nvs corrupt ", 12) == 0) {
    nvsCorrupt(line + 12);
  } else if (strcmp(line, "crash") == 0) {
    LOG_W("crash: aborting on purpose");
    Serial.flush();
    abort();
  } else if (strcmp(line, "crash hang") == 0) {
    LOG_W("crash: spinning without feeding the watchdog (expect TASK_WDT in ~10 s)");
    Serial.flush();
    for (;;) {
    }
#endif
  } else if (strcmp(line, "help") == 0) {
    Serial.println(
        "commands: shot (stream frame buffer), heap (diagnostics line), "
        "ai [easy|medium|hard] (search initial position), aibench [check N], help"
#ifdef CROWBOARD_DEBUG_CMDS
        ", nvs corrupt <ns> <key>, crash [hang]"
#endif
    );
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
  _benchStep = -1;  // a manual request ends a running benchmark
  const uint32_t id = nextAiId();
  if (_ai->request(checkers::makeRequest(checkers::Board::initial(), d, id, true, millis()))) {
    LOG_I("ai: request %u submitted", (unsigned)id);
  } else {
    _aiPending = false;
    LOG_E("ai: request failed (worker not running)");
  }
}

// Issues the next id in this client's range and marks it as awaited.
uint32_t SerialCmd::nextAiId() {
  _aiSeq = (_aiSeq + 1) & checkers::kAiIdSeqMask;
  if (_aiSeq == 0) _aiSeq = 1;
  _aiId = checkers::kAiIdSerial | _aiSeq;
  _aiPending = true;
  return _aiId;
}

void SerialCmd::aiBenchStart(const char* arg) {
  if (!_ai) {
    LOG_W("aibench: worker not attached");
    return;
  }
  _benchCheck = checkers::kAiNodeCheckInterval;
  if (strncmp(arg, "check ", 6) == 0) _benchCheck = (uint32_t)strtoul(arg + 6, nullptr, 10);
  LOG_I("aibench: Searcher %u B in %s, TT %u entries, check every %u nodes, heap %u",
        (unsigned)sizeof(checkers::Searcher), _ai->searcherInternal() ? "internal" : "PSRAM",
        (unsigned)_ai->ttEntries(), (unsigned)_benchCheck, (unsigned)ESP.getFreeHeap());
  _benchStep = 0;
  _benchStartMs = millis();
  aiBenchNext();
}

void SerialCmd::aiBenchNext() {
  const BenchRun& run = kBenchRuns[_benchStep];
  checkers::Board b = checkers::Board::initial();
  if (run.mid) checkers::Board::fromString(kBenchMid, b);
  checkers::AiRequest r;
  if (run.profile >= 0) {
    r = checkers::makeRequest(b, (checkers::Difficulty)run.profile, 0, true, 1, true);
  } else {
    r = checkers::makeRequest(b, checkers::Difficulty::Hard, 0, true, 1, true);
    r.budgetMs = 0;
    r.maxDepth = kBenchFixedDepth;
    r.useTT = run.profile == -2;
  }
  r.nodeCheckInterval = _benchCheck;
  r.requestId = nextAiId();
  if (!_ai->request(r)) _aiPending = false;
}

void SerialCmd::aiPoll() {
  // Only our own result is taken: the Play scene's results stay in the worker's slot for it.
  checkers::AiResult r;
  if (!_ai || !_aiPending || !_ai->pollFor(_aiId, r)) return;
  _aiPending = false;
  if (_benchStep >= 0) {
    const BenchRun& run = kBenchRuns[_benchStep];
    const uint32_t nps = r.elapsedMs ? (uint32_t)((uint64_t)r.nodes * 1000 / r.elapsedMs) : 0;
    LOG_I("aibench: %-7s %-6s depth %2d nodes %7u %5u ms %6u nodes/s completed %d stack %u",
          run.mid ? "mid" : "initial", run.label, r.depth, (unsigned)r.nodes, (unsigned)r.elapsedMs,
          (unsigned)nps, (int)r.completed, (unsigned)_ai->stackHighWaterMark());
    if (++_benchStep < kBenchCount) {
      aiBenchNext();
    } else {
      _benchStep = -1;
      LOG_I("aibench: done in %u ms, heap %u (min %u)", (unsigned)(millis() - _benchStartMs),
            (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMinFreeHeap());
    }
    return;
  }
  LOG_I("ai: id %u move %u->%u depth %d score %d nodes %u %u ms completed %d, stack free %u B",
        (unsigned)r.requestId, r.move.from + 1, r.move.to + 1, r.depth, r.score, (unsigned)r.nodes,
        (unsigned)r.elapsedMs, (int)r.completed, (unsigned)_ai->stackHighWaterMark());
}

#ifdef CROWBOARD_DEBUG_CMDS
// Flips one payload byte of a stored blob and writes it back, so Blob::load sees a CRC mismatch.
void SerialCmd::nvsCorrupt(const char* arg) {
  char ns[IStorage::kMaxNameLen + 1] = {0};
  char key[IStorage::kMaxNameLen + 1] = {0};
  if (!_storage || sscanf(arg, "%15s %15s", ns, key) != 2) {
    LOG_W("nvs corrupt: usage 'nvs corrupt <ns> <key>' (e.g. settings v1)");
    return;
  }
  uint8_t buf[1100];
  size_t len = 0;
  if (!_storage->get(ns, key, buf, sizeof(buf), &len) || len == 0) {
    LOG_W("nvs corrupt: %s/%s not found", ns, key);
    return;
  }
  buf[len - 1] ^= 0xFF;  // last byte: always payload (or header) so the CRC cannot still match
  if (_storage->put(ns, key, buf, len)) {
    LOG_I("nvs corrupt: flipped last byte of %s/%s (%u bytes); reboot to load it", ns, key,
          (unsigned)len);
  } else {
    LOG_E("nvs corrupt: write back failed");
  }
}
#endif
