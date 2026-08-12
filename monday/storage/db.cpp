// monday/storage/db.cpp
//
// SQLCipher encrypted database management.

#include "monday/storage/db.h"
#include "monday/security/keystore.h"
#include "monday/core/log.h"
#include "monday/core/allocator.h"
#include <string.h>
#include <stdlib.h>

#ifdef MONDAY_WINDOWS
#include <windows.h>
#else
#include <sodium.h>
#endif

extern "C" {
int sqlite3_key(sqlite3 *db, const void *zKey, int nKey);
}

struct monday_db {
    sqlite3 *sqlite;
    uint8_t  key[32];
};

extern "C" {

monday_error_t monday_db_open(const char *db_path, const uint8_t derived_key[32], monday_db_t **out_db) {
    if (!db_path || !out_db) {
        return MONDAY_ERROR(MONDAY_ERR_INVALID_ARG, "null arguments to db_open");
    }

    monday_db_t *db = (monday_db_t*)monday_zone_alloc(MONDAY_ZONE_STATIC, sizeof(monday_db_t));
    if (!db) {
        return MONDAY_ERROR(MONDAY_ERR_OUT_OF_MEMORY, "alloc db structure failed");
    }
    memset(db, 0, sizeof(monday_db_t));

    // Resolve key
    if (derived_key) {
        memcpy(db->key, derived_key, 32);
    } else {
        // Keystore-only mode: retrieve or generate key
        monday_error_t keystore_err = monday_keystore_get("monday.db.key", db->key, 32);
        if (keystore_err.code == MONDAY_ERR_NOT_FOUND) {
            // Generate a random key
#ifdef MONDAY_WINDOWS
            HCRYPTPROV hProv;
            if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT) ||
                !CryptGenRandom(hProv, 32, db->key)) {
                return MONDAY_ERROR(MONDAY_ERR_PERMISSION_DENIED, "failed to generate random key");
            }
            CryptReleaseContext(hProv, 0);
#else
            if (sodium_init() < 0) {
                return MONDAY_ERROR(MONDAY_ERR_INFERENCE_FAILED, "libsodium init failed");
            }
            randombytes_buf(db->key, 32);
#endif
            // Save to keystore
            keystore_err = monday_keystore_set("monday.db.key", db->key, 32);
            if (keystore_err.code != MONDAY_OK) {
                return keystore_err;
            }
        } else if (keystore_err.code != MONDAY_OK) {
            return keystore_err;
        }
    }

    // Open SQLite database connection
    int res = sqlite3_open_v2(db_path, &db->sqlite, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX, NULL);
    if (res != SQLITE_OK) {
        sqlite3_close(db->sqlite);
        return MONDAY_ERROR(MONDAY_ERR_DB_OPEN_FAILED, "failed to open sqlite database");
    }

    // Key the database using SQLCipher C API
    res = sqlite3_key(db->sqlite, db->key, 32);
    if (res != SQLITE_OK) {
        sqlite3_close(db->sqlite);
        return MONDAY_ERROR(MONDAY_ERR_DB_WRONG_KEY, "database keying failed");
    }

    // Set cipher WAL legacy mode to 0
    sqlite3_exec(db->sqlite, "PRAGMA cipher_wal_legacy = 0;", NULL, NULL, NULL);

    // Test the key by running a simple query
    char *err_msg = NULL;
    res = sqlite3_exec(db->sqlite, "SELECT count(*) FROM sqlite_schema;", NULL, NULL, &err_msg);
    if (res != SQLITE_OK) {
        if (err_msg) sqlite3_free(err_msg);
        sqlite3_close(db->sqlite);
        if (res == SQLITE_NOTADB || res == SQLITE_PERM || res == SQLITE_AUTH) {
            return MONDAY_ERROR(MONDAY_ERR_DB_WRONG_KEY, "wrong database key");
        }
        return MONDAY_ERROR(MONDAY_ERR_DB_CORRUPT, "database decryption failed");
    }

    // Enable WAL mode
    res = sqlite3_exec(db->sqlite, "PRAGMA journal_mode = WAL;", NULL, NULL, &err_msg);
    if (res != SQLITE_OK) {
        if (err_msg) sqlite3_free(err_msg);
        sqlite3_close(db->sqlite);
        return MONDAY_ERROR(MONDAY_ERR_DB_OPEN_FAILED, "failed to enable WAL");
    }

    sqlite3_exec(db->sqlite, "PRAGMA synchronous = NORMAL;", NULL, NULL, NULL);

    *out_db = db;
    return MONDAY_ERROR(MONDAY_OK, "");
}

void monday_db_close(monday_db_t *db) {
    if (!db || !db->sqlite) return;

    // Checkpoint WAL changes to the main file
    sqlite3_exec(db->sqlite, "PRAGMA wal_checkpoint(TRUNCATE);", NULL, NULL, NULL);

    sqlite3_close(db->sqlite);
    db->sqlite = nullptr;
    memset(db->key, 0, 32);
}

sqlite3 *monday_db_handle(monday_db_t *db) {
    return db ? db->sqlite : nullptr;
}

monday_error_t monday_db_exec(monday_db_t *db, const char *sql) {
    if (!db || !db->sqlite || !sql) {
        return MONDAY_ERROR(MONDAY_ERR_INVALID_ARG, "null database/sql parameters");
    }

    char *err_msg = NULL;
    int res = sqlite3_exec(db->sqlite, sql, NULL, NULL, &err_msg);
    if (res != SQLITE_OK) {
        monday_error_t err;
        if (res == SQLITE_CORRUPT) {
            err = MONDAY_ERROR(MONDAY_ERR_DB_CORRUPT, err_msg ? err_msg : "database corrupt");
        } else {
            err = MONDAY_ERROR(MONDAY_ERR_DB_OPEN_FAILED, err_msg ? err_msg : "query execution failed");
        }
        if (err_msg) sqlite3_free(err_msg);
        return err;
    }

    return MONDAY_ERROR(MONDAY_OK, "");
}

} // extern "C"
