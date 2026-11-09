#include "hal/BootInfo.h"

#include <esp_system.h>
#include <stdio.h>
#include <stdlib.h>

#include "hal/Log.h"

#if defined(__has_include)
#if __has_include(<esp_core_dump.h>)
#include <esp_core_dump.h>
#define CB_HAVE_COREDUMP 1
#endif
#endif

namespace checkers::hal {

static const char* resetReasonText(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON:
      return "POWERON";
    case ESP_RST_EXT:
      return "EXT";
    case ESP_RST_SW:
      return "SW";
    case ESP_RST_PANIC:
      return "PANIC";
    case ESP_RST_INT_WDT:
      return "INT_WDT";
    case ESP_RST_TASK_WDT:
      return "TASK_WDT";
    case ESP_RST_WDT:
      return "WDT";
    case ESP_RST_DEEPSLEEP:
      return "DEEPSLEEP";
    case ESP_RST_BROWNOUT:
      return "BROWNOUT";
    case ESP_RST_SDIO:
      return "SDIO";
    default:
      return "UNKNOWN";
  }
}

void logBootInfo() {
  const esp_reset_reason_t r = esp_reset_reason();
  const bool abnormal = r == ESP_RST_PANIC || r == ESP_RST_INT_WDT || r == ESP_RST_TASK_WDT ||
                        r == ESP_RST_WDT || r == ESP_RST_BROWNOUT;
  if (abnormal) {
    LOG_W("boot: reset reason %s (%d)", resetReasonText(r), (int)r);
  } else {
    LOG_I("boot: reset reason %s (%d)", resetReasonText(r), (int)r);
  }

#if defined(CB_HAVE_COREDUMP) && CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH
  if (esp_core_dump_image_check() != ESP_OK) return;  // none present (or invalid)
#if CONFIG_ESP_COREDUMP_DATA_FORMAT_ELF
  auto* s = static_cast<esp_core_dump_summary_t*>(malloc(sizeof(esp_core_dump_summary_t)));
  if (s && esp_core_dump_get_summary(s) == ESP_OK) {
    s->exc_task[sizeof(s->exc_task) - 1] = '\0';
    LOG_E("coredump: task '%s' pc 0x%08x cause %u vaddr 0x%08x", s->exc_task, (unsigned)s->exc_pc,
          (unsigned)s->ex_info.exc_cause, (unsigned)s->ex_info.exc_vaddr);
    char reason[96] = {0};
    if (esp_core_dump_get_panic_reason(reason, sizeof(reason)) == ESP_OK) {
      LOG_E("coredump: reason '%s'", reason);
    }
    char bt[16 * 9 + 1] = {0};
    size_t n = 0;
    for (uint32_t i = 0; i < s->exc_bt_info.depth && i < 16 && n + 9 < sizeof(bt); ++i) {
      n += snprintf(bt + n, sizeof(bt) - n, "%08x ", (unsigned)s->exc_bt_info.bt[i]);
    }
    LOG_E("coredump: backtrace %s", bt);
  } else {
    LOG_E("coredump: present but summary unreadable");
  }
  free(s);
#else
  LOG_E("coredump: present (summary needs ELF format)");
#endif
  // Erase so the same crash is reported once and a later clean boot does not re-log it.
  if (esp_core_dump_image_erase() != ESP_OK) LOG_W("coredump: erase failed");
#endif
}

}  // namespace checkers::hal
