// Tiny line-oriented serial command parser. Call tick() once per loop iteration.
//
//   shot  stream the frame buffer: "SHOT <w> <h> RGB565RLE <bytes>\n", <bytes> of payload,
//         then "\nEND\n". Payload is runs of 3 bytes: pixel byte 0, pixel byte 1, count (1..255).
//         readRect returns byte-swapped RGB565, so byte 0 is the high byte of the
//         true RGB565 value (big-endian on the wire).
//   heap  print the diagnostics line now
//   help  list commands
//
// Further commands can be attached with setHandler() (main.cpp wires the rendering experiments
// this way) so this class stays free of ui/ dependencies.
#pragma once

#include <Arduino.h>
#include "crowpanel7_lgfx.h"
#include "AiWorker.h"
#include "Diag.h"

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

  // Enables the "ai [easy|medium|hard]" command.
  void attachAi(checkers::hal::AiWorker* ai) { _ai = ai; }

 private:
  static constexpr size_t kMaxLine = 32;

  LGFX& _gfx;
  Diag& _diag;
  Handler _handler = nullptr;
  void* _ctx = nullptr;
  char _line[kMaxLine];
  size_t _len = 0;
  checkers::hal::AiWorker* _ai = nullptr;
  uint32_t _aiId = 0;

  void handle(const char* line);
  void shot();
  void aiRequest(const char* arg);
  void aiPoll();
};
