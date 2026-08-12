// monday/goap/extractors_generated.c
//
// Generated parameter extractor dispatch table (Architecture §2.5).

#include "monday/goap/extractors.h"
#include <string.h>

typedef monday_error_t (*monday_extractor_fn_t)(const char *query, monday_extractor_value_t *out_val);

typedef struct monday_extractor_entry {
    const char           *extractor_name;
    monday_extractor_fn_t fn;
} monday_extractor_entry_t;

static const monday_extractor_entry_t g_extractor_table[] = {
    {"parse_duration_from_query", parse_duration_from_query},
    {"parse_filename_from_query", parse_filename_from_query},
    {"parse_search_query",       parse_search_query},
    {NULL, NULL}
};

monday_error_t monday_dispatch_extractor(const char *extractor_name,
                                         const char *query,
                                         monday_extractor_value_t *out_val) {
    if (!extractor_name || !query || !out_val) {
        return MONDAY_ERROR(MONDAY_ERR_INVALID_ARG, "null extractor dispatch arg");
    }

    for (size_t i = 0; g_extractor_table[i].extractor_name != NULL; i++) {
        if (strcmp(g_extractor_table[i].extractor_name, extractor_name) == 0) {
            return g_extractor_table[i].fn(query, out_val);
        }
    }

    return MONDAY_ERROR(MONDAY_ERR_NOT_FOUND, "extractor function not found");
}
