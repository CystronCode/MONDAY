// monday/engram/episodic_store.cpp
//
// Episodic memory SQLCipher store.

#include "monday/engram/episodic_store.h"
#include "monday/security/keystore.h"
#include "monday/core/allocator.h"
#include "monday/core/log.h"
#include <sodium.h>
#include <string.h>
#include <stdlib.h>

static uint8_t s_episodic_key[32] = {0};

static void compute_record_hmac(const monday_episodic_record_t *record, uint8_t out_hmac[32]) {
    size_t sess_len = strlen(record->session_id);
    size_t sum_len = record->summary ? strlen(record->summary) : 0;
    size_t conv_len = record->conversation ? strlen(record->conversation) : 0;
    size_t total_len = 8 + sess_len + sum_len + conv_len;

    uint8_t *buf = (uint8_t*)malloc(total_len);
    if (!buf) {
        memset(out_hmac, 0, 32);
        return;
    }

    // Store big-endian turn_id
    for (int i = 0; i < 8; i++) {
        buf[i] = (uint8_t)(record->turn_id >> ((7 - i) * 8));
    }

    if (sess_len > 0) {
        memcpy(buf + 8, record->session_id, sess_len);
    }
    if (sum_len > 0) {
        memcpy(buf + 8 + sess_len, record->summary, sum_len);
    }
    if (conv_len > 0) {
        memcpy(buf + 8 + sess_len + sum_len, record->conversation, conv_len);
    }

    crypto_auth_hmacsha256(out_hmac, buf, total_len, s_episodic_key);
    free(buf);
}

extern "C" {

monday_error_t monday_episodic_store_init(void) {
    if (sodium_init() < 0) {
        return MONDAY_ERROR(MONDAY_ERR_INFERENCE_FAILED, "libsodium init failed");
    }

    // Load or generate the HMAC signature key
    monday_error_t key_err = monday_keystore_get("monday.episodic.key", s_episodic_key, 32);
    if (key_err.code == MONDAY_ERR_NOT_FOUND) {
        randombytes_buf(s_episodic_key, 32);
        key_err = monday_keystore_set("monday.episodic.key", s_episodic_key, 32);
        if (key_err.code != MONDAY_OK) {
            return key_err;
        }
    } else if (key_err.code != MONDAY_OK) {
        return key_err;
    }

    return MONDAY_ERROR(MONDAY_OK, "");
}

monday_error_t monday_episodic_store_insert(monday_db_t *db, const monday_episodic_record_t *record) {
    if (!db || !record) {
        return MONDAY_ERROR(MONDAY_ERR_INVALID_ARG, "null db or record");
    }

    uint8_t hmac_sig[32];
    compute_record_hmac(record, hmac_sig);

    sqlite3 *sqlite = monday_db_handle(db);
    sqlite3_stmt *stmt = NULL;
    const char *sql = "INSERT OR REPLACE INTO episodic_memory "
                      "(turn_id, session_id, summary, conversation, hmac_sig, timestamp) "
                      "VALUES (?, ?, ?, ?, ?, ?);";

    int res = sqlite3_prepare_v2(sqlite, sql, -1, &stmt, NULL);
    if (res != SQLITE_OK) {
        return MONDAY_ERROR(MONDAY_ERR_DB_OPEN_FAILED, "failed to prepare insert statement");
    }

    sqlite3_bind_int64(stmt, 1, record->turn_id);
    sqlite3_bind_text(stmt, 2, record->session_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, record->summary ? record->summary : "", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, record->conversation ? record->conversation : "", -1, SQLITE_TRANSIENT);
    sqlite3_bind_blob(stmt, 5, hmac_sig, 32, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 6, (sqlite3_int64)record->timestamp);

    res = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (res != SQLITE_DONE) {
        return MONDAY_ERROR(MONDAY_ERR_DB_OPEN_FAILED, "failed to insert episodic record");
    }

    return MONDAY_ERROR(MONDAY_OK, "");
}

monday_error_t monday_episodic_store_get(monday_db_t *db, int64_t turn_id, monday_episodic_record_t *out_record) {
    if (!db || !out_record) {
        return MONDAY_ERROR(MONDAY_ERR_INVALID_ARG, "null db or out_record");
    }

    sqlite3 *sqlite = monday_db_handle(db);
    sqlite3_stmt *stmt = NULL;
    const char *sql = "SELECT session_id, summary, conversation, hmac_sig, timestamp "
                      "FROM episodic_memory WHERE turn_id = ?;";

    int res = sqlite3_prepare_v2(sqlite, sql, -1, &stmt, NULL);
    if (res != SQLITE_OK) {
        return MONDAY_ERROR(MONDAY_ERR_DB_OPEN_FAILED, "failed to prepare select statement");
    }

    sqlite3_bind_int64(stmt, 1, turn_id);

    res = sqlite3_step(stmt);
    if (res != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return MONDAY_ERROR(MONDAY_ERR_NOT_FOUND, "episodic record not found");
    }

    const char *sess_id = (const char*)sqlite3_column_text(stmt, 0);
    const char *summary = (const char*)sqlite3_column_text(stmt, 1);
    const char *conversation = (const char*)sqlite3_column_text(stmt, 2);
    const void *recorded_hmac = sqlite3_column_blob(stmt, 3);
    sqlite3_int64 timestamp = sqlite3_column_int64(stmt, 4);

    // Populate temporary record to verify HMAC
    monday_episodic_record_t temp_rec;
    temp_rec.turn_id = turn_id;
    strncpy(temp_rec.session_id, sess_id ? sess_id : "", sizeof(temp_rec.session_id)-1);
    temp_rec.session_id[sizeof(temp_rec.session_id)-1] = '\0';
    temp_rec.summary = (char*)(summary ? summary : "");
    temp_rec.conversation = (char*)(conversation ? conversation : "");
    temp_rec.timestamp = (time_t)timestamp;

    uint8_t computed_hmac[32];
    compute_record_hmac(&temp_rec, computed_hmac);

    if (memcmp(recorded_hmac, computed_hmac, 32) != 0) {
        sqlite3_finalize(stmt);
        char err_msg[128];
        snprintf(err_msg, sizeof(err_msg), "episodic_store: HMAC verification FAILED for turn_id %ld", (long)turn_id);
        monday_log_write(MONDAY_LOG_SYSTEM, err_msg);
        return MONDAY_ERROR(MONDAY_ERR_DB_INTEGRITY_FAILED, "episodic record signature mismatch (quarantined)");
    }

    // Allocation via general allocator (monday_alloc)
    out_record->turn_id = turn_id;
    strcpy(out_record->session_id, temp_rec.session_id);
    out_record->summary = (char*)monday_alloc(strlen(temp_rec.summary) + 1);
    out_record->conversation = (char*)monday_alloc(strlen(temp_rec.conversation) + 1);
    out_record->timestamp = temp_rec.timestamp;

    if (!out_record->summary || !out_record->conversation) {
        if (out_record->summary) monday_free(out_record->summary);
        if (out_record->conversation) monday_free(out_record->conversation);
        sqlite3_finalize(stmt);
        return MONDAY_ERROR(MONDAY_ERR_OUT_OF_MEMORY, "failed to allocate memory for retrieved record");
    }

    strcpy(out_record->summary, temp_rec.summary);
    strcpy(out_record->conversation, temp_rec.conversation);

    sqlite3_finalize(stmt);
    return MONDAY_ERROR(MONDAY_OK, "");
}

void monday_episodic_record_free(monday_episodic_record_t *record) {
    if (!record) return;
    if (record->summary) {
        monday_free(record->summary);
        record->summary = nullptr;
    }
    if (record->conversation) {
        monday_free(record->conversation);
        record->conversation = nullptr;
    }
}

} // extern "C"
