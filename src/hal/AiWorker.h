// Checkers search on core 0 (ARCHITECTURE §9).
//
// request() overwrites any pending request (queue of depth 1) and aborts the search in flight;
// poll() / pollFor() are non-blocking. Every request produces exactly one result, even when
// cancelled, into a single result slot that the next result overwrites. The worker is shared by
// the Play scene (moves and hints) and the serial `ai` / `aibench` commands, so each client uses
// its own request-id range (game/AiRequest.h) and takes only its own results with pollFor(); a
// result nobody waits for stays in the slot until the next one replaces it.
//
// Memory (allocated once by begin(), see docs/ai-notes.md): the Searcher (~131 KB) and a 1 MB
// transposition table, used by requests with useTT (Hard) and cleared on newGame or a change of
// forcedCapture. Where each lives is set by AI_SEARCHER_INTERNAL / AI_TT_BYTES below.
#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include <atomic>

// Arduino.h defines a bit(b) macro that collides with checkers::bit() in game/Board.h.
#pragma push_macro("bit")
#undef bit
#include "game/AiRequest.h"
#pragma pop_macro("bit")

namespace checkers::hal {

class AiWorker {
 public:
  static constexpr uint32_t kStackBytes = 24 * 1024;

  // Allocates the Searcher, transposition table, queues and task. Returns false (and stays
  // unusable) on failure; a missing table only disables useTT.
  bool begin();

  // Queues a request, replacing a pending one and aborting a running search. False before begin().
  bool request(const AiRequest& req);

  // Takes the finished result if there is one, whoever asked for it. Never blocks.
  bool poll(AiResult& out);

  // Takes the finished result only if it answers `requestId`; a result for another client is
  // left in the slot. Never blocks.
  bool pollFor(uint32_t requestId, AiResult& out);

  // True while a finished result waits in the slot.
  bool hasResult() const;

  // Aborts the running or pending search. Its (partial) result is still delivered by poll().
  void cancel();

  // True from request() until the matching result is queued for poll().
  bool busy() const;

  // Smallest amount of free stack (bytes) seen on the worker task so far; 0 before begin().
  uint32_t stackHighWaterMark() const { return stackHwm_.load(); }

  // Transposition table entries (0 if none) and whether the Searcher is in internal RAM.
  size_t ttEntries() const { return tt_.entries(); }
  bool searcherInternal() const { return searcherInternal_; }

 private:
  struct Msg {
    AiRequest req;
    uint32_t gen;  // cancel generation stamped by request()
  };
  struct StopCtx {
    const AiWorker* self;
    uint32_t gen;
    uint32_t startMs;
    uint32_t budgetMs;
  };

  static void taskEntry(void* arg);
  static bool shouldStop(void* ctx);
  void loop();

  Searcher* searcher_ = nullptr;
  bool searcherInternal_ = false;
  TranspositionTable tt_;
  AiTable table_;  // touched by the worker task only (after begin())
  QueueHandle_t reqQ_ = nullptr;
  QueueHandle_t resQ_ = nullptr;
  TaskHandle_t task_ = nullptr;
  std::atomic<uint32_t> cancelGen_{0};  // bumped by cancel() and request()
  std::atomic<bool> running_{false};    // worker holds a request it has not answered yet
  std::atomic<uint32_t> stackHwm_{0};
};

}  // namespace checkers::hal
