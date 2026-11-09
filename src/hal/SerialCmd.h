// Tiny line-oriented serial command parser. Call tick() once per loop iteration.
//
//   shot  stream the frame buffer: "SHOT <w> <h> RGB565RLE <bytes>\n", <bytes> of payload,
//         then "\nEND\n". Payload is runs of 3 bytes: pixel byte 0, pixel byte 1, count (1..255).
//         readRect returns byte-swapped RGB565, so byte 0 is the high byte of the
//         true RGB565 value (big-endian on the wire).
//   heap  print the diagnostics line now
//   ai [easy|medium|hard]   search the initial position with that difficulty profile
//   aibench [check N]       benchmark: initial and a mid-game position, each at fixed depth 6
//                           (without and with the TT) and under each profile's budget; one
//                           "aibench:" line per run, then a summary (docs/ai-notes.md). N is the
//                           stop-check interval in nodes (default kAiNodeCheckInterval).
//   help  list commands
//   Only with -DCROWBOARD_DEBUG_CMDS (fault injection, docs/robustness-checklist.md):
//   nvs corrupt <ns> <key>  flip one payload byte of a stored blob (its CRC then fails)
//   crash [hang]            software panic (abort); "hang" spins the loop to trip the task WDT
//
// Further commands can be attached with setHandler() (main.cpp wires the rendering experiments
// this way) so this class stays free of ui/ dependencies.
#pragma once

#include <Arduino.h>
#include "crowpanel7_lgfx.h"
#include "AiWorker.h"
#include "Diag.h"
#include "services/IStorage.h"

class SerialCmd {
 public:
  // Gets every line this class does not handle itself and returns true if it consumed it. It
  // also receives "help" (after the built-in list is printed) so it can list its own commands.
  using Handler = bool (*)(const char* line, void* ctx);

  SerialCmd(LGFX& gfx, Diag& diag) : _gfx(gfx), _diag(diag) {}

  void tick();
  void setHandler(Handler handler, void* ctx) {
    _handler = handler;
    _ctx = ctx;
  }

  // Enables the "ai [easy|medium|hard]" and "aibench" commands.
  void attachAi(checkers::hal::AiWorker* ai) { _ai = ai; }
  // Storage used by the debug "nvs corrupt" command (ignored without CROWBOARD_DEBUG_CMDS).
  void attachStorage(IStorage* storage) { _storage = storage; }

 private:
  static constexpr size_t kMaxLine = 64;

  LGFX& _gfx;
  Diag& _diag;
  Handler _handler = nullptr;
  void* _ctx = nullptr;
  char _line[kMaxLine];
  size_t _len = 0;
  checkers::hal::AiWorker* _ai = nullptr;
  IStorage* _storage = nullptr;
  uint32_t _aiId = 0;
  // aibench: index of the run in flight (-1 = idle) and its stop-check interval.
  int _benchStep = -1;
  uint32_t _benchCheck = 0;
  uint32_t _benchStartMs = 0;

  void handle(const char* line);
  void shot();
  void aiRequest(const char* arg);
  void aiPoll();
  void aiBenchStart(const char* arg);
  void aiBenchNext();
  void nvsCorrupt(const char* arg);
};
