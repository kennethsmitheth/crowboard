#include "AiWorker.h"

#include <esp_heap_caps.h>

#include <new>

#include "Log.h"

// Experiment switches (docs/ai-notes.md). Defaults are the measured best configuration.
#ifndef AI_SEARCHER_INTERNAL
#define AI_SEARCHER_INTERNAL 0  // 1: Searcher in internal RAM, 0: in PSRAM
#endif
#ifndef AI_TT_BYTES
#define AI_TT_BYTES (1024 * 1024)  // transposition table in PSRAM; 0 = none
#endif

#ifndef AI_EVAL_MOBILITY
#define AI_EVAL_MOBILITY (EvalWeights{}.mobility)  // eval mobility weight
#endif

namespace checkers::hal {

namespace {

EvalWeights workerWeights() {
  EvalWeights w;
  w.mobility = AI_EVAL_MOBILITY;
  return w;
}

}  // namespace

bool AiWorker::begin() {
  if (task_) return true;
  void* mem = nullptr;
  if (AI_SEARCHER_INTERNAL) {
    mem = heap_caps_malloc(sizeof(Searcher), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    searcherInternal_ = mem != nullptr;
  }
  if (!mem) mem = heap_caps_malloc(sizeof(Searcher), MALLOC_CAP_SPIRAM);
  if (!mem) {
    LOG_E("AiWorker: no memory for Searcher (%u bytes)", (unsigned)sizeof(Searcher));
    return false;
  }
  searcher_ = new (mem) Searcher();
  if (AI_TT_BYTES > 0) {
    void* ttMem = heap_caps_malloc(AI_TT_BYTES, MALLOC_CAP_SPIRAM);
    if (ttMem && tt_.init(ttMem, AI_TT_BYTES)) {
      table_.tt = &tt_;
    } else {
      LOG_W("AiWorker: no PSRAM for the transposition table, Hard runs without it");
      heap_caps_free(ttMem);
    }
  }
  reqQ_ = xQueueCreate(1, sizeof(Msg));
  resQ_ = xQueueCreate(1, sizeof(AiResult));
  if (!reqQ_ || !resQ_ ||
      xTaskCreatePinnedToCore(&AiWorker::taskEntry, "ai", kStackBytes, this, 1, &task_, 0) !=
          pdPASS) {
    LOG_E("AiWorker: queue/task creation failed");
    task_ = nullptr;
    return false;
  }
  LOG_I("AiWorker: ready, Searcher %u bytes in %s, TT %u entries, stack %u",
        (unsigned)sizeof(Searcher), searcherInternal_ ? "internal RAM" : "PSRAM",
        (unsigned)tt_.entries(), (unsigned)kStackBytes);
  return true;
}

bool AiWorker::request(const AiRequest& req) {
  if (!task_) return false;
  Msg m{req, cancelGen_.fetch_add(1) + 1};  // aborts a search that is still running
  running_.store(true);
  xQueueOverwrite(reqQ_, &m);
  return true;
}

bool AiWorker::poll(AiResult& out) {
  return task_ && xQueueReceive(resQ_, &out, 0) == pdTRUE;
}

void AiWorker::cancel() {
  cancelGen_.fetch_add(1);
}

bool AiWorker::busy() const {
  return task_ && (running_.load() || uxQueueMessagesWaiting(reqQ_) > 0);
}

void AiWorker::taskEntry(void* arg) {
  static_cast<AiWorker*>(arg)->loop();
}

bool AiWorker::shouldStop(void* ctx) {
  const StopCtx* c = static_cast<const StopCtx*>(ctx);
  if (c->self->cancelGen_.load() != c->gen) return true;
  return c->budgetMs != 0 && (uint32_t)(millis() - c->startMs) >= c->budgetMs;
}

void AiWorker::loop() {
  for (;;) {
    Msg m;
    // Peek first so busy() never sees an empty queue with running_ still false.
    xQueuePeek(reqQ_, &m, portMAX_DELAY);
    running_.store(true);
    xQueueReceive(reqQ_, &m, 0);  // newest request wins if one overwrote the peeked one

    table_.prepare(*searcher_, m.req);  // a 1 MB clear (~ms) is not charged to the budget
    StopCtx ctx{this, m.gen, millis(), m.req.budgetMs};
    SearchResult s = runAiSearch(*searcher_, m.req, workerWeights(), &AiWorker::shouldStop, &ctx);
    AiResult r = toAiResult(s, m.req, millis() - ctx.startMs);

    stackHwm_.store(uxTaskGetStackHighWaterMark(nullptr));
    LOG_D("AiWorker: id %u depth %d nodes %u %u ms completed %d, stack free min %u bytes",
          (unsigned)r.requestId, r.depth, (unsigned)r.nodes, (unsigned)r.elapsedMs,
          (int)r.completed, (unsigned)stackHwm_.load());

    xQueueOverwrite(resQ_, &r);
    // Stay busy if a newer request is already waiting.
    running_.store(uxQueueMessagesWaiting(reqQ_) > 0);
  }
}

}  // namespace checkers::hal
