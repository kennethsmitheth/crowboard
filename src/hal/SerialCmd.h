// Tiny line-oriented serial command parser. Call tick() once per loop iteration.
//
//   shot  stream the frame buffer: "SHOT <w> <h> RGB565RLE <bytes>\n", <bytes> of payload,
//         then "\nEND\n". Payload is runs of 3 bytes: pixel byte 0, pixel byte 1, count (1..255).
//         readRect returns byte-swapped RGB565, so byte 0 is the high byte of the
//         true RGB565 value (big-endian on the wire).
//   heap  print the diagnostics line now
//   help  list commands
#pragma once

#include <Arduino.h>
#include "crowpanel7_lgfx.h"
#include "Diag.h"

class SerialCmd {
 public:
  SerialCmd(LGFX& gfx, Diag& diag) : _gfx(gfx), _diag(diag) {}

  void tick();

 private:
  static constexpr size_t kMaxLine = 32;

  LGFX& _gfx;
  Diag& _diag;
  char _line[kMaxLine];
  size_t _len = 0;

  void handle(const char* line);
  void shot();
};
