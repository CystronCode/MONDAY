// monday/engram/engram.cpp
//
// ENGRAM Memory Engine coordinator implementation.

#include "monday/engram/engram.h"
#include "monday/engram/episodic_store.h"
#include "monday/core/allocator.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

extern "C" {

monday_error_t monday_engram_init(void) {
    return monday_episodic_store_init();
}

monday_error_t monday_engram_commit_turn(monday_db_t *db,
                                         int64_t turn_id,
                                         const char *session_id,
                                         const char *user_query,
                                         const char *llm_response) {
    if (!db || !session_id || !user_query || !llm_response) {
        return MONDAY_ERROR(MONDAY_ERR_INVALID_ARG, "null args to commit_turn");
    }

    // Format conversation transcript
    size_t q_len = strlen(user_query);
    size_t r_len = strlen(llm_response);
    size_t conv_len = q_len + r_len + 32; // buffer size for "User: \nMonday: \n"
    
    char *conversation = (char*)malloc(conv_len);
    if (!conversation) {
        return MONDAY_ERROR(MONDAY_ERR_OUT_OF_MEMORY, "alloc conversation transcript failed");
    }
    
    snprintf(conversation, conv_len, "User: %s\nMonday: %s", user_query, llm_response);

    monday_episodic_record_t rec;
    rec.turn_id = turn_id;
    strncpy(rec.session_id, session_id, sizeof(rec.session_id)-1);
    rec.session_id[sizeof(rec.session_id)-1] = '\0';
    rec.summary = (char*)"need_summary"; // Trigger background summariser
    rec.conversation = conversation;
    rec.timestamp = time(NULL);

    monday_error_t err = monday_episodic_store_insert(db, &rec);

    free(conversation);
    return err;
}

} // extern "C"
