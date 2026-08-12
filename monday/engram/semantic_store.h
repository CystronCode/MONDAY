// monday/engram/semantic_store.h
//
// Semantic memory store (FTS5 + Vector storage).

#pragma once

#include "monday/storage/db.h"
#include "monday/core/errors.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int64_t  chunk_id;
    char     doc_id[128];
    char    *text_content;  // heap-allocated via monday_alloc()
    float   *embedding;     // heap-allocated via monday_alloc()
    size_t   embedding_dim;
    int64_t  timestamp;
} monday_semantic_record_t;

// Insert a semantic chunk with its text and vector embedding.
// Automatically adds it to semantic_memory and the FTS5 fts_idx virtual table.
monday_error_t monday_semantic_store_insert(monday_db_t *db,
                                            const char *doc_id,
                                            const char *text_content,
                                            const float *embedding,
                                            size_t embedding_dim);

// Query using FTS5 full-text match. Returns candidates for re-ranking.
monday_error_t monday_semantic_store_query_fts(monday_db_t *db,
                                               const char *query_text,
                                               monday_semantic_record_t **out_records,
                                               size_t *out_count);

// Free the allocated array of records.
void monday_semantic_records_free(monday_semantic_record_t *records, size_t count);

#ifdef __cplusplus
}
#endif
