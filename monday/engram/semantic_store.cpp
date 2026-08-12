// monday/engram/semantic_store.cpp
//
// Semantic memory store implementation (FTS5 + SQLCipher blob).

#include "monday/engram/semantic_store.h"
#include "monday/core/allocator.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <vector>

extern "C" {

monday_error_t monday_semantic_store_insert(monday_db_t *db,
                                            const char *doc_id,
                                            const char *text_content,
                                            const float *embedding,
                                            size_t embedding_dim) {
    if (!db || !doc_id || !text_content || !embedding || embedding_dim == 0) {
        return MONDAY_ERROR(MONDAY_ERR_INVALID_ARG, "invalid semantic insert arguments");
    }

    sqlite3 *sqlite = monday_db_handle(db);
    sqlite3_exec(sqlite, "BEGIN TRANSACTION;", NULL, NULL, NULL);

    // 1. Insert into semantic_memory
    sqlite3_stmt *stmt1 = NULL;
    const char *sql1 = "INSERT INTO semantic_memory (doc_id, text_content, embedding, timestamp) "
                       "VALUES (?, ?, ?, ?);";
    int res = sqlite3_prepare_v2(sqlite, sql1, -1, &stmt1, NULL);
    if (res != SQLITE_OK) {
        sqlite3_exec(sqlite, "ROLLBACK;", NULL, NULL, NULL);
        return MONDAY_ERROR(MONDAY_ERR_DB_OPEN_FAILED, "failed to prepare semantic_memory insert");
    }

    time_t now = time(NULL);
    sqlite3_bind_text(stmt1, 1, doc_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt1, 2, text_content, -1, SQLITE_TRANSIENT);
    sqlite3_bind_blob(stmt1, 3, embedding, (int)(embedding_dim * sizeof(float)), SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt1, 4, (sqlite3_int64)now);

    res = sqlite3_step(stmt1);
    sqlite3_finalize(stmt1);

    if (res != SQLITE_DONE) {
        sqlite3_exec(sqlite, "ROLLBACK;", NULL, NULL, NULL);
        return MONDAY_ERROR(MONDAY_ERR_DB_OPEN_FAILED, "failed to execute semantic_memory insert");
    }

    // Get the generated chunk ID
    int64_t chunk_id = sqlite3_last_insert_rowid(sqlite);

    // 2. Insert into FTS5 index (fts_idx)
    sqlite3_stmt *stmt2 = NULL;
    const char *sql2 = "INSERT INTO fts_idx (chunk_id, text_content) VALUES (?, ?);";
    res = sqlite3_prepare_v2(sqlite, sql2, -1, &stmt2, NULL);
    if (res != SQLITE_OK) {
        sqlite3_exec(sqlite, "ROLLBACK;", NULL, NULL, NULL);
        return MONDAY_ERROR(MONDAY_ERR_DB_OPEN_FAILED, "failed to prepare fts_idx insert");
    }

    sqlite3_bind_int64(stmt2, 1, chunk_id);
    sqlite3_bind_text(stmt2, 2, text_content, -1, SQLITE_TRANSIENT);

    res = sqlite3_step(stmt2);
    sqlite3_finalize(stmt2);

    if (res != SQLITE_DONE) {
        sqlite3_exec(sqlite, "ROLLBACK;", NULL, NULL, NULL);
        return MONDAY_ERROR(MONDAY_ERR_DB_OPEN_FAILED, "failed to execute fts_idx insert");
    }

    sqlite3_exec(sqlite, "COMMIT;", NULL, NULL, NULL);
    return MONDAY_ERROR(MONDAY_OK, "");
}

monday_error_t monday_semantic_store_query_fts(monday_db_t *db,
                                               const char *query_text,
                                               monday_semantic_record_t **out_records,
                                               size_t *out_count) {
    if (!db || !query_text || !out_records || !out_count) {
        return MONDAY_ERROR(MONDAY_ERR_INVALID_ARG, "invalid semantic query arguments");
    }

    sqlite3 *sqlite = monday_db_handle(db);
    sqlite3_stmt *stmt = NULL;

    // Join search to fetch complete record fields
    const char *sql = "SELECT f.chunk_id, s.doc_id, s.text_content, s.embedding, s.timestamp "
                      "FROM fts_idx f "
                      "JOIN semantic_memory s ON f.chunk_id = s.chunk_id "
                      "WHERE f.text_content MATCH ? LIMIT 50;";

    int res = sqlite3_prepare_v2(sqlite, sql, -1, &stmt, NULL);
    if (res != SQLITE_OK) {
        return MONDAY_ERROR(MONDAY_ERR_DB_OPEN_FAILED, "failed to prepare fts join query");
    }

    sqlite3_bind_text(stmt, 1, query_text, -1, SQLITE_TRANSIENT);

    // Count first to allocate buffer
    size_t count = 0;
    std::vector<monday_semantic_record_t> temp_list;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        monday_semantic_record_t rec;
        rec.chunk_id = sqlite3_column_int64(stmt, 0);
        
        const char *doc_id = (const char*)sqlite3_column_text(stmt, 1);
        strncpy(rec.doc_id, doc_id ? doc_id : "", sizeof(rec.doc_id)-1);
        rec.doc_id[sizeof(rec.doc_id)-1] = '\0';

        const char *text = (const char*)sqlite3_column_text(stmt, 2);
        const void *emb_blob = sqlite3_column_blob(stmt, 3);
        int emb_bytes = sqlite3_column_bytes(stmt, 3);
        rec.timestamp = (int64_t)sqlite3_column_int64(stmt, 4);

        // Allocate using general allocator
        rec.text_content = (char*)monday_alloc(strlen(text) + 1);
        strcpy(rec.text_content, text);

        rec.embedding_dim = emb_bytes / sizeof(float);
        rec.embedding = (float*)monday_alloc(emb_bytes);
        memcpy(rec.embedding, emb_blob, emb_bytes);

        temp_list.push_back(rec);
    }
    sqlite3_finalize(stmt);

    count = temp_list.size();
    if (count == 0) {
        *out_records = nullptr;
        *out_count = 0;
        return MONDAY_ERROR(MONDAY_OK, "");
    }

    // Allocate array
    monday_semantic_record_t *records = (monday_semantic_record_t*)monday_alloc(count * sizeof(monday_semantic_record_t));
    if (!records) {
        for (auto &r : temp_list) {
            monday_free(r.text_content);
            monday_free(r.embedding);
        }
        return MONDAY_ERROR(MONDAY_ERR_OUT_OF_MEMORY, "failed to allocate query results buffer");
    }

    for (size_t i = 0; i < count; i++) {
        records[i] = temp_list[i];
    }

    *out_records = records;
    *out_count = count;

    return MONDAY_ERROR(MONDAY_OK, "");
}

void monday_semantic_records_free(monday_semantic_record_t *records, size_t count) {
    if (!records) return;
    for (size_t i = 0; i < count; i++) {
        if (records[i].text_content) monday_free(records[i].text_content);
        if (records[i].embedding) monday_free(records[i].embedding);
    }
    monday_free(records);
}

} // extern "C"
