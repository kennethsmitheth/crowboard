// Task watchdog on the Arduino loop task.
//
// watchdogBegin() subscribes the calling task (call it at the end of setup()) with a 10 s timeout
// (kWatchdogTimeoutMs). The loop calls feedWatchdog() once per iteration; code that blocks the
// loop for longer than a frame (serial bench/anim commands) calls it inside its own loops. If the
// loop stalls for 10 s the task WDT panics, a core dump is written and the next boot logs it.
#pragma once

#include <stdint.h>

namespace checkers::hal {

constexpr uint32_t kWatchdogTimeoutMs = 10000;

// Returns false if the watchdog could not be configured (loop then runs unwatched).
bool watchdogBegin();
// Safe to call before watchdogBegin().
void feedWatchdog();

}  // namespace checkers::hal
