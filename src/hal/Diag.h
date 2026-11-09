// Periodic diagnostics line: heap, PSRAM, loop timing and loop rate.
//
// Call tick() once per loop iteration. Every 5 seconds it prints one line via LOG_I.
#pragma once

#include <Arduino.h>

class Diag {
public:
    void tick();

    // Print the diagnostics line now and restart the report window.
    void report();

private:
    static constexpr uint32_t kReportIntervalMs = 5000;

    bool _started = false;
    uint32_t _lastUs = 0;        // micros() at the previous tick
    uint32_t _windowStartMs = 0; // millis() at the start of the current report window
    uint32_t _frames = 0;        // tick-to-tick intervals in the current window
    uint64_t _sumUs = 0;         // sum of those intervals
    uint32_t _worstUs = 0;       // longest of those intervals
};
