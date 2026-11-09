#include "Diag.h"

#include "Log.h"

void Diag::tick() {
  const uint32_t nowUs = micros();
  const uint32_t nowMs = millis();

  if (!_started) {
    _started = true;
    _lastUs = nowUs;
    _windowStartMs = nowMs;
    return;
  }

  // Interval between consecutive ticks is one full loop iteration.
  const uint32_t dtUs = nowUs - _lastUs;
  _lastUs = nowUs;
  _sumUs += dtUs;
  if (dtUs > _worstUs) {
    _worstUs = dtUs;
  }
  _frames++;

  if (nowMs - _windowStartMs >= kReportIntervalMs) {
    report();
  }
}

void Diag::report() {
  const uint32_t elapsedMs = millis() - _windowStartMs;

  // Loop times in tenths of a millisecond, integer math only (no float printf).
  const uint32_t avgTenthsMs = _frames ? (uint32_t)(_sumUs / _frames / 100) : 0;
  const uint32_t worstTenthsMs = _worstUs / 100;
  const uint32_t loopsPerSec = elapsedMs ? (uint32_t)((uint64_t)_frames * 1000 / elapsedMs) : 0;

  // Least free stack the loop task has had so far (bytes; the ESP-IDF port counts in bytes).
  // The deepest path is an autosave at the end of a slide: two 1 KB blob buffers plus NVS.
  const unsigned long stackFree = (unsigned long)uxTaskGetStackHighWaterMark(nullptr);

  LOG_I(
      "diag heap %lu (min %lu) psram %lu | loop avg %lu.%lu ms worst %lu.%lu ms | %lu loops/s | "
      "stack min free %lu | touch: %s | storage: %s",
      (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getMinFreeHeap(),
      (unsigned long)ESP.getFreePsram(), (unsigned long)(avgTenthsMs / 10),
      (unsigned long)(avgTenthsMs % 10), (unsigned long)(worstTenthsMs / 10),
      (unsigned long)(worstTenthsMs % 10), (unsigned long)loopsPerSec, stackFree,
      _touchPresent ? "ok" : "absent", _storagePersistent ? "nvs" : "RAM-ONLY");

  _windowStartMs = millis();
  _frames = 0;
  _sumUs = 0;
  _worstUs = 0;
}
