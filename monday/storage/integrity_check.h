// monday/storage/integrity_check.h
//
// Integrity check APIs for SQLCipher database.

#pragma once

#include "monday/storage/db.h"
#include "monday/core/errors.h"

#ifdef __cplusplus
extern "C" {
#endif

// Run PRAGMA integrity_check on the database.
// Returns MONDAY_OK on success (the query returned "ok").
// Returns MONDAY_ERR_DB_CORRUPT if errors are found.
monday_error_t monday_db_integrity_check(monday_db_t *db);

#ifdef __cplusplus
}
#endif
