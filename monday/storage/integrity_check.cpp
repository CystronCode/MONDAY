// monday/storage/integrity_check.cpp
//
// PRAGMA integrity_check execution.

#include "monday/storage/integrity_check.h"
#include <string.h>

extern "C" {

monday_error_t monday_db_integrity_check(monday_db_t *db) {
    if (!db) {
        return MONDAY_ERROR(MONDAY_ERR_INVALID_ARG, "null db connection");
    }

    sqlite3 *sqlite = monday_db_handle(db);
    sqlite3_stmt *stmt = NULL;
    int res = sqlite3_prepare_v2(sqlite, "PRAGMA integrity_check;", -1, &stmt, NULL);
    if (res != SQLITE_OK) {
        return MONDAY_ERROR(MONDAY_ERR_DB_CORRUPT, "failed to prepare integrity check statement");
    }

    bool ok = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char *text = sqlite3_column_text(stmt, 0);
        if (text && strcmp((const char*)text, "ok") == 0) {
            ok = true;
        }
    }

    sqlite3_finalize(stmt);

    if (ok) {
        return MONDAY_ERROR(MONDAY_OK, "");
    } else {
        return MONDAY_ERROR(MONDAY_ERR_DB_CORRUPT, "PRAGMA integrity_check failed — database is corrupt");
    }
}

} // extern "C"
