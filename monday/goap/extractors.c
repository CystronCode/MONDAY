// monday/goap/extractors.c
//
// Parameter extractor implementations for GOAP intent parameters.

#include "monday/goap/extractors.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

monday_error_t parse_duration_from_query(const char *query, monday_extractor_value_t *out_val) {
    if (!query || !out_val) {
        return MONDAY_ERROR(MONDAY_ERR_INVALID_ARG, "null extractor arg");
    }

    out_val->type = EXTRACTOR_TYPE_INT;
    out_val->val.i64 = 600; /* Default 10 minutes */

    int val = 0;
    char unit[64] = {0};
    if (sscanf(query, "%*[^0123456789]%d %63s", &val, unit) >= 1 || sscanf(query, "%d %63s", &val, unit) >= 1) {
        if (val > 0) {
            if (strstr(unit, "sec") || strstr(unit, "second")) {
                out_val->val.i64 = val;
            } else if (strstr(unit, "hour") || strstr(unit, "hr")) {
                out_val->val.i64 = (int64_t)val * 3600;
            } else {
                /* Minute default */
                out_val->val.i64 = (int64_t)val * 60;
            }
        }
    }

    return MONDAY_ERROR(MONDAY_OK, "duration parsed");
}

monday_error_t parse_filename_from_query(const char *query, monday_extractor_value_t *out_val) {
    if (!query || !out_val) {
        return MONDAY_ERROR(MONDAY_ERR_INVALID_ARG, "null extractor arg");
    }

    out_val->type = EXTRACTOR_TYPE_STRING;
    snprintf(out_val->val.str, sizeof(out_val->val.str), "document.txt");

    const char *p = strstr(query, "file ");
    if (!p) p = strstr(query, "document ");
    if (p) {
        p = strchr(p, ' ');
        if (p && *(p + 1) != '\0') {
            snprintf(out_val->val.str, sizeof(out_val->val.str), "%s", p + 1);
        }
    }

    return MONDAY_ERROR(MONDAY_OK, "filename parsed");
}

monday_error_t parse_search_query(const char *query, monday_extractor_value_t *out_val) {
    if (!query || !out_val) {
        return MONDAY_ERROR(MONDAY_ERR_INVALID_ARG, "null extractor arg");
    }

    out_val->type = EXTRACTOR_TYPE_STRING;
    snprintf(out_val->val.str, sizeof(out_val->val.str), "%s", query);
    return MONDAY_ERROR(MONDAY_OK, "search query parsed");
}
