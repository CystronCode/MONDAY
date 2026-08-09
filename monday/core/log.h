// monday/core/log.h
#pragma once
#include <stddef.h>

typedef enum {
    MONDAY_LOG_SYSTEM      = 0,
    MONDAY_LOG_DIAGNOSTIC  = 1,
    MONDAY_LOG_VERBOSE     = 2,
    MONDAY_LOG_SENSITIVE   = 3,
} monday_log_level_t;

// Initialise logger. path = log file path. max_size_bytes = rotation trigger.
// generations = how many rotated files to keep.
// Returns 0 on success, -1 on error.
int monday_log_init(const char *path, size_t max_size_bytes, int generations);

// Write one structured log line (NDJSON). Thread-safe.
void monday_log_write(monday_log_level_t level, const char *msg);

// Write with an extra key=value pair of type long.
void monday_log_write_kv_long(monday_log_level_t level,
                               const char *msg,
                               const char *key,
                               long value);

// Flush and close the logger.
void monday_log_close(void);
