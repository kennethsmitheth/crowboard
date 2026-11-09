// Boot diagnostics: reset reason and any core dump left by a previous crash.
#pragma once

namespace checkers::hal {

// Logs esp_reset_reason() as text. If a core dump is present on the coredump partition, logs its
// summary (task, PC, exception cause) once and erases it. Call early in setup().
void logBootInfo();

}  // namespace checkers::hal
