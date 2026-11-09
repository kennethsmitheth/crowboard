// Serial logging with compile-time level filtering.
//
//   LOG_LEVEL 0 = off, 1 = errors, 2 = warnings, 3 = info (default), 4 = debug.
//
// Override from the build with e.g. -DLOG_LEVEL=0. Filtered macros expand to nothing,
// so their arguments are not evaluated. Output is printf-style into Serial's own buffer,
// so no heap is allocated per message.
#pragma once

#include <Arduino.h>

#ifndef LOG_LEVEL
#define LOG_LEVEL 3
#endif

// Set while a binary stream (screenshot) owns Serial; log lines are dropped meanwhile.
inline bool g_logMuted = false;

#define LOG_PRINT_(tag, fmt, ...)                                         \
  do {                                                                    \
    if (!g_logMuted) Serial.printf("[" tag "] " fmt "\n", ##__VA_ARGS__); \
  } while (0)

#if LOG_LEVEL >= 1
#define LOG_E(fmt, ...) LOG_PRINT_("E", fmt, ##__VA_ARGS__)
#else
#define LOG_E(fmt, ...) ((void)0)
#endif

#if LOG_LEVEL >= 2
#define LOG_W(fmt, ...) LOG_PRINT_("W", fmt, ##__VA_ARGS__)
#else
#define LOG_W(fmt, ...) ((void)0)
#endif

#if LOG_LEVEL >= 3
#define LOG_I(fmt, ...) LOG_PRINT_("I", fmt, ##__VA_ARGS__)
#else
#define LOG_I(fmt, ...) ((void)0)
#endif

#if LOG_LEVEL >= 4
#define LOG_D(fmt, ...) LOG_PRINT_("D", fmt, ##__VA_ARGS__)
#else
#define LOG_D(fmt, ...) ((void)0)
#endif
