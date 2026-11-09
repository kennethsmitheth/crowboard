#include "hal/Watchdog.h"

#include <esp_task_wdt.h>

#include "hal/Log.h"

namespace checkers::hal {

static bool g_subscribed = false;

bool watchdogBegin() {
  esp_task_wdt_config_t cfg = {};
  cfg.timeout_ms = kWatchdogTimeoutMs;
#ifdef CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0
  cfg.idle_core_mask |= (1u << 0);
#endif
#ifdef CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU1
  cfg.idle_core_mask |= (1u << 1);
#endif
  cfg.trigger_panic = true;
  esp_err_t err = esp_task_wdt_reconfigure(&cfg);
  if (err == ESP_ERR_INVALID_STATE) err = esp_task_wdt_init(&cfg);  // not initialised yet
  if (err != ESP_OK) {
    LOG_W("wdt: configure failed (%d); loop unwatched", (int)err);
    return false;
  }
  err = esp_task_wdt_add(nullptr);
  if (err != ESP_OK && err != ESP_ERR_INVALID_ARG) {  // INVALID_ARG: already subscribed
    LOG_W("wdt: subscribe failed (%d); loop unwatched", (int)err);
    return false;
  }
  g_subscribed = true;
  LOG_I("wdt: loop task watched, timeout %u ms", (unsigned)kWatchdogTimeoutMs);
  return true;
}

void feedWatchdog() {
  if (g_subscribed) esp_task_wdt_reset();
}

}  // namespace checkers::hal
