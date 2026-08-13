/* AUTO-GENERATED — DO NOT EDIT — see tools/generate_blackboard.py */
#pragma once
#include <stdint.h>
#include <stddef.h>

int64_t monday_bb_get_turn_id(void);
void    monday_bb_set_turn_id(int64_t v);

const char *monday_bb_get_user_query(void);
void        monday_bb_set_user_query(const char *v, size_t len);

const char *monday_bb_get_goap_action(void);
void        monday_bb_set_goap_action(const char *v, size_t len);

const char *monday_bb_get_llm_response(void);
void        monday_bb_set_llm_response(const char *v, size_t len);

const char *monday_bb_get_session_id(void);
void        monday_bb_set_session_id(const char *v, size_t len);
