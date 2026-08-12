// monday/engram/episodic_store.h
//
// Episodic memory SQLCipher store.
// Architecture §3.6: Episodic memory has HMAC verification on read.

#pragma once

#include "monday/storage/db.h"
#include "monday/core/errors.h"
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int64_t  turn_id;
    char     session_id[64];
    char    *summary;      // heap-allocated via monday_alloc()
    char    *conversation; // heap-allocated via monday_alloc()
    time_t   timestamp;
} monday_episodic_record_t;

// Initialize the episodic store. Loads or generates the HMAC signature key.
monday_error_t monday_episodic_store_init(void);

// Insert an episodic record into the database. Calculates HMAC signature before writing.
monday_error_t monday_episodic_store_insert(monday_db_t *db, const monday_episodic_record_t *record);

// Retrieve an episodic record by turn_id. Verifies the HMAC signature.
// Returns MONDAY_ERR_DB_INTEGRITY_FAILED if signature verification fails.
monday_error_t monday_episodic_store_get(monday_db_t *db, int64_t turn_id, monday_episodic_record_t *out_record);

// Free string fields inside the episodic record.
void monday_episodic_record_free(monday_episodic_record_t *record);

#ifdef __cplusplus
}
#endif
