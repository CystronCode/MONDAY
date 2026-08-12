// monday/engram/engram.h
//
// ENGRAM Memory Engine coordinator.

#pragma once

#include "monday/storage/db.h"
#include "monday/core/errors.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize the engram system.
monday_error_t monday_engram_init(void);

// Commit a completed conversation turn to the episodic memory store.
// Automatically signs the record with HMAC.
// The summary is initially marked as needing consolidation.
monday_error_t monday_engram_commit_turn(monday_db_t *db,
                                         int64_t turn_id,
                                         const char *session_id,
                                         const char *user_query,
                                         const char *llm_response);

#ifdef __cplusplus
}
#endif
