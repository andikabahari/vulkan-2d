#pragma once

// Basic Includes
// -----------------------------------------------------------------------------

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

// Codebase Keywords
// -----------------------------------------------------------------------------

#define internal      static
#define global        static
#define local_persist static

// Basic Types
// -----------------------------------------------------------------------------

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;

typedef s8       b8;
typedef s16      b16;
typedef s32      b32;
typedef s64      b64;

typedef float    f32;
typedef double   f64;

// Assert
// -----------------------------------------------------------------------------

#define ASSERT(expr)        \
    do {                    \
        if (!(expr)) {      \
            __debugbreak(); \
        }                   \
    } while (0)

// Utils
// -----------------------------------------------------------------------------

#define ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))

// Log
// -----------------------------------------------------------------------------

enum Log_Level : u8 {
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_FATAL,
};

internal void log_printf(Log_Level level, const char *fmt, ...);

#define LOG_DEBUG(fmt, ...) log_printf(LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__);
#define LOG_INFO(fmt, ...) log_printf(LOG_LEVEL_INFO, fmt, ##__VA_ARGS__);
#define LOG_WARNING(fmt, ...) log_printf(LOG_LEVEL_WARNING, fmt, ##__VA_ARGS__);
#define LOG_ERROR(fmt, ...) log_printf(LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__);
#define LOG_FATAL(fmt, ...)                              \
    do {                                                 \
        log_printf(LOG_LEVEL_FATAL, fmt, ##__VA_ARGS__); \
        exit(1);                                         \
    } while (0)
