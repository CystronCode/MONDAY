/* AUTO-GENERATED — DO NOT EDIT — see tools/generate_blackboard.py */
#include "blackboard_accessors.h"
#include <string.h>
#include <stdlib.h>

static int64_t g_bb_turn_id = 0;
int64_t monday_bb_get_turn_id(void) { return g_bb_turn_id; }
void    monday_bb_set_turn_id(int64_t v) { g_bb_turn_id = v; }

static char g_bb_user_query[4096] = {0};
const char *monday_bb_get_user_query(void) { return g_bb_user_query; }
void monday_bb_set_user_query(const char *v, size_t len) {
    size_t copy = len < 4096 - 1 ? len : 4096 - 1;
    memcpy(g_bb_user_query, v, copy);
    g_bb_user_query[copy] = '\0';
}

static char g_bb_goap_action[256] = {0};
const char *monday_bb_get_goap_action(void) { return g_bb_goap_action; }
void monday_bb_set_goap_action(const char *v, size_t len) {
    size_t copy = len < 256 - 1 ? len : 256 - 1;
    memcpy(g_bb_goap_action, v, copy);
    g_bb_goap_action[copy] = '\0';
}

static char g_bb_llm_response[65536] = {0};
const char *monday_bb_get_llm_response(void) { return g_bb_llm_response; }
void monday_bb_set_llm_response(const char *v, size_t len) {
    size_t copy = len < 65536 - 1 ? len : 65536 - 1;
    memcpy(g_bb_llm_response, v, copy);
    g_bb_llm_response[copy] = '\0';
}

static char g_bb_session_id[64] = {0};
const char *monday_bb_get_session_id(void) { return g_bb_session_id; }
void monday_bb_set_session_id(const char *v, size_t len) {
    size_t copy = len < 64 - 1 ? len : 64 - 1;
    memcpy(g_bb_session_id, v, copy);
    g_bb_session_id[copy] = '\0';
}
