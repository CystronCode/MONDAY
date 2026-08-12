// monday/storage/migrations.h
//
// Schema migrations runner.

#pragma once

#include "monday/storage/db.h"
#include "monday/core/errors.h"

#ifdef __cplusplus
extern "C" {
#endif

// Run all pending migrations on the given database.
monday_error_t monday_migrations_run(monday_db_t *db);

#ifdef __cplusplus
}
#endif
