// monday/storage/db.h
//
// SQLCipher encrypted database management interface.
// Architecture §3.7: transparent AES-256 page-level encryption, WAL mode.

#pragma once

#include <sqlite3.h>
#include <stdint.h>
#include "monday/core/errors.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct monday_db monday_db_t;

// Open the SQLCipher-encrypted database at db_path.
// If derived_key is non-NULL, it is used directly (Argon2id path).
// If derived_key is NULL, the engine retrieves/generates "monday.db.key" (Keystore-only mode).
// Returns MONDAY_ERR_DB_WRONG_KEY if the key is incorrect.
monday_error_t monday_db_open(const char *db_path, const uint8_t derived_key[32], monday_db_t **out_db);

// Close the database, checkpointing the WAL file before unlinking.
void monday_db_close(monday_db_t *db);

// Returns the underlying sqlite3 handle.
sqlite3 *monday_db_handle(monday_db_t *db);

// Helper to execute non-query SQL commands.
monday_error_t monday_db_exec(monday_db_t *db, const char *sql);

#ifdef __cplusplus
}
#endif
