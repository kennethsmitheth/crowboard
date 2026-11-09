// Checkers search on core 0 (ARCHITECTURE §9).
//
// request() overwrites any pending request (queue of depth 1) and aborts the search in flight;
// poll() is non-blocking. Every request produces exactly one result, even when cancelled: the
// caller drops results whose requestId is not the one it is waiting for. The Searcher
// (~131 KB) lives in PSRAM, allocated once by begin().
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

  // Allocates the Searcher, queues and task. Returns false (and stays unusable) on failure.
  bool begin();

  // Queues a request, replacing a pending one and aborting a running search. False before begin().
  bool request(const AiRequest& req);

  // Takes the finished result if there is one. Never blocks.
  bool poll(AiResult& out);

  // Aborts the running or pending search. Its (partial) result is still delivered by poll().
  void cancel();

  // True from request() until the matching result is queued for poll().
  bool busy() const;

  // Smallest amount of free stack (bytes) seen on the worker task so far; 0 before begin().
  uint32_t stackHighWaterMark() const { return stackHwm_.load(); }

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
  QueueHandle_t reqQ_ = nullptr;
  QueueHandle_t resQ_ = nullptr;
  TaskHandle_t task_ = nullptr;
  std::atomic<uint32_t> cancelGen_{0};  // bumped by cancel() and request()
  std::atomic<bool> running_{false};    // worker holds a request it has not answered yet
  std::atomic<uint32_t> stackHwm_{0};
};

}  // namespace checkers::hal
