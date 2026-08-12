// monday/engram/summariser.cpp
//
// Background episodic and semantic summarisation pipeline.

#include "monday/engram/summariser.h"
#include "monday/engram/episodic_store.h"
#include "monday/core/log.h"
#include "monday/core/allocator.h"
#include <atomic>
#include <fstream>
#include <sstream>
#include <string>
#include <string.h>

static std::atomic<bool> g_summarise_cancelled(false);

static std::string read_prompt_file(const char *path) {
    std::ifstream file(path);
    if (!file.is_open()) return "";
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

static std::string replace_string(std::string str, const std::string &from, const std::string &to) {
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
    return str;
}

namespace monday {

void monday_summariser_cancel(void) {
    g_summarise_cancelled.store(true);
}

monday_error_t monday_summariser_consolidate(IInferenceEngine *engine, monday_db_t *db) {
    if (!engine || !db) {
        return MONDAY_ERROR(MONDAY_ERR_INVALID_ARG, "null engine or db");
    }

    sqlite3 *sqlite = monday_db_handle(db);
    sqlite3_stmt *stmt = NULL;

    // Find the first turn requiring consolidation
    // We treat empty or placeholder summary as needing summarization
    const char *query_sql = "SELECT turn_id, session_id, conversation, timestamp FROM episodic_memory "
                            "WHERE summary = '' OR summary = 'need_summary' LIMIT 1;";
    
    int res = sqlite3_prepare_v2(sqlite, query_sql, -1, &stmt, NULL);
    if (res != SQLITE_OK) {
        return MONDAY_ERROR(MONDAY_ERR_DB_OPEN_FAILED, "failed to query unsummarized turns");
    }

    res = sqlite3_step(stmt);
    if (res != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return MONDAY_ERROR(MONDAY_OK, "no turns requiring consolidation");
    }

    int64_t turn_id = sqlite3_column_int64(stmt, 0);
    const char *sess_id = (const char*)sqlite3_column_text(stmt, 1);
    const char *conversation = (const char*)sqlite3_column_text(stmt, 2);
    int64_t timestamp = sqlite3_column_int64(stmt, 3);

    std::string session_id_str = sess_id ? sess_id : "";
    std::string conv_str = conversation ? conversation : "";
    
    sqlite3_finalize(stmt);

    // Read the template prompt from data/prompts/summarise_episodic.txt
    std::string prompt_template = read_prompt_file("data/prompts/summarise_episodic.txt");
    if (prompt_template.empty()) {
        return MONDAY_ERROR(MONDAY_ERR_NOT_FOUND, "consolidate prompt template not found");
    }

    std::string prompt = replace_string(prompt_template, "{episodic_chunk}", conv_str);

    // Reset cancel flag
    g_summarise_cancelled.store(false);

    // Setup inference request
    InferenceRequest req = {};
    req.prompt = prompt.c_str();
    req.max_new_tokens = 200; // max summary output: 200 tokens
    req.temperature = 0.3f;   // low temperature for facts summary
    req.top_p = 0.9f;
    req.append_to_kv = false;  // run in isolated context

    std::string summary_accum;

    req.on_token = [&summary_accum](int32_t tok_id, const char *tok_text) -> bool {
        if (tok_id == -1) return false;
        if (g_summarise_cancelled.load()) {
            return false; // Cancel execution
        }
        if (tok_text) {
            summary_accum += tok_text;
        }
        return true;
    };

    monday_log_write(MONDAY_LOG_DIAGNOSTIC, "summariser: starting background turn consolidation");
    
    auto inf_res = engine->run_inference(req);

    if (g_summarise_cancelled.load()) {
        monday_log_write(MONDAY_LOG_DIAGNOSTIC, "summariser: consolidation task cancelled mid-inference");
        return MONDAY_ERROR(MONDAY_ERR_CANCELLED, "summarisation cancelled");
    }

    if (!inf_res) {
        return MONDAY_ERROR(MONDAY_ERR_INFERENCE_FAILED, "summarisation inference failed");
    }

    // Insert updated summary record with computed HMAC
    monday_episodic_record_t rec;
    rec.turn_id = turn_id;
    strncpy(rec.session_id, session_id_str.c_str(), sizeof(rec.session_id)-1);
    rec.session_id[sizeof(rec.session_id)-1] = '\0';
    rec.summary = (char*)summary_accum.c_str();
    rec.conversation = (char*)conv_str.c_str();
    rec.timestamp = (time_t)timestamp;

    // Use episodic_store to safely sign and write back to SQLite
    monday_error_t insert_err = monday_episodic_store_insert(db, &rec);
    if (insert_err.code != MONDAY_OK) {
        return insert_err;
    }

    monday_log_write_kv_long(MONDAY_LOG_DIAGNOSTIC, "summariser: turn consolidation complete", "turn_id", (long)turn_id);

    return MONDAY_ERROR(MONDAY_OK, "");
}

} // namespace monday
