// monday/goap/extractors.h
//
// Parameter extractor functions registered for GOAP intent parameters.

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "monday/core/errors.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct monday_extractor_value {
    enum {
        EXTRACTOR_TYPE_INT = 1,
        EXTRACTOR_TYPE_STRING = 2,
        EXTRACTOR_TYPE_FLOAT = 3,
        EXTRACTOR_TYPE_BOOL = 4
    } type;
    union {
        int64_t i64;
        double  f64;
        bool    b;
        char    str[256];
    } val;
} monday_extractor_value_t;

/* Parse duration from query string (e.g., "10 minutes" -> 600s, "30 seconds" -> 30s). */
monday_error_t parse_duration_from_query(const char *query, monday_extractor_value_t *out_val);

/* Parse filename from query string (e.g., "open report.pdf" -> "report.pdf"). */
monday_error_t parse_filename_from_query(const char *query, monday_extractor_value_t *out_val);

/* Parse search query from utterance string. */
monday_error_t parse_search_query(const char *query, monday_extractor_value_t *out_val);

#ifdef __cplusplus
}
#endif
