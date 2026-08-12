// monday/storage/migrations.cpp
//
// Database migrations runner.

#include "monday/storage/migrations.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static const char *s_migration_v1 =
    "CREATE TABLE IF NOT EXISTS config ("
    "    key   TEXT PRIMARY KEY,"
    "    value TEXT NOT NULL"
    ");"
    ""
    "CREATE TABLE IF NOT EXISTS permission_grants ("
    "    id           INTEGER PRIMARY KEY AUTOINCREMENT,"
    "    action_class TEXT    NOT NULL,"
    "    target_scope TEXT    NOT NULL,"
    "    granted_at   INTEGER NOT NULL,"
    "    expires_at   INTEGER NOT NULL,"
    "    session_id   TEXT    NOT NULL"
    ");"
    "CREATE INDEX IF NOT EXISTS idx_grants_target ON permission_grants(action_class, target_scope);"
    ""
    "CREATE TABLE IF NOT EXISTS episodic_memory ("
    "    turn_id       INTEGER PRIMARY KEY,"
    "    session_id    TEXT    NOT NULL,"
    "    summary       TEXT    NOT NULL,"
    "    conversation  TEXT    NOT NULL,"
    "    hmac_sig      BLOB    NOT NULL,"
    "    timestamp     INTEGER NOT NULL"
    ");"
    ""
    "CREATE TABLE IF NOT EXISTS semantic_memory ("
    "    chunk_id      INTEGER PRIMARY KEY AUTOINCREMENT,"
    "    doc_id        TEXT    NOT NULL,"
    "    text_content  TEXT    NOT NULL,"
    "    embedding     BLOB    NOT NULL,"
    "    timestamp     INTEGER NOT NULL"
    ");"
    ""
    "CREATE VIRTUAL TABLE IF NOT EXISTS fts_idx USING fts5("
    "    chunk_id UNINDEXED,"
    "    text_content"
    ");";

extern "C" {

monday_error_t monday_migrations_run(monday_db_t *db) {
    if (!db) {
        return MONDAY_ERROR(MONDAY_ERR_INVALID_ARG, "null db connection");
    }

    sqlite3 *sqlite = monday_db_handle(db);

    // Create schema_version table if it does not exist
    const char *create_version_table =
        "CREATE TABLE IF NOT EXISTS schema_version ("
        "    version INTEGER PRIMARY KEY,"
        "    applied_at INTEGER NOT NULL"
        ");";
    
    char *err_msg = NULL;
    int res = sqlite3_exec(sqlite, create_version_table, NULL, NULL, &err_msg);
    if (res != SQLITE_OK) {
        monday_error_t err = MONDAY_ERROR(MONDAY_ERR_DB_MIGRATION_FAILED, err_msg ? err_msg : "failed to create schema_version table");
        if (err_msg) sqlite3_free(err_msg);
        return err;
    }

    // Get current version
    int current_version = 0;
    sqlite3_stmt *stmt = NULL;
    res = sqlite3_prepare_v2(sqlite, "SELECT MAX(version) FROM schema_version;", -1, &stmt, NULL);
    if (res == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            current_version = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    } else {
        return MONDAY_ERROR(MONDAY_ERR_DB_MIGRATION_FAILED, "failed to query schema version");
    }

    // Execute migrations in a transaction
    if (current_version < 1) {
        res = sqlite3_exec(sqlite, "BEGIN TRANSACTION;", NULL, NULL, NULL);
        if (res != SQLITE_OK) {
            return MONDAY_ERROR(MONDAY_ERR_DB_MIGRATION_FAILED, "failed to start transaction");
        }

        res = sqlite3_exec(sqlite, s_migration_v1, NULL, NULL, &err_msg);
        if (res != SQLITE_OK) {
            sqlite3_exec(sqlite, "ROLLBACK;", NULL, NULL, NULL);
            monday_error_t err = MONDAY_ERROR(MONDAY_ERR_DB_MIGRATION_FAILED, err_msg ? err_msg : "failed to run migration v1");
            if (err_msg) sqlite3_free(err_msg);
            return err;
        }

        // Record migration version 1
        time_t now = time(NULL);
        sqlite3_stmt *upd_stmt = NULL;
        res = sqlite3_prepare_v2(sqlite, "INSERT INTO schema_version (version, applied_at) VALUES (1, ?);", -1, &upd_stmt, NULL);
        if (res == SQLITE_OK) {
            sqlite3_bind_int64(upd_stmt, 1, (sqlite3_int64)now);
            sqlite3_step(upd_stmt);
            sqlite3_finalize(upd_stmt);
        } else {
            sqlite3_exec(sqlite, "ROLLBACK;", NULL, NULL, NULL);
            return MONDAY_ERROR(MONDAY_ERR_DB_MIGRATION_FAILED, "failed to record migration completion");
        }

        res = sqlite3_exec(sqlite, "COMMIT;", NULL, NULL, NULL);
        if (res != SQLITE_OK) {
            return MONDAY_ERROR(MONDAY_ERR_DB_MIGRATION_FAILED, "failed to commit migration transaction");
        }
    }

    return MONDAY_ERROR(MONDAY_OK, "");
}

} // extern "C"
