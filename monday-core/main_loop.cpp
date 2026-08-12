#include "monday-core/main_loop.h"
#include "monday-core/shutdown.h"
#include "monday/core/allocator.h"
#include "monday/core/log.h"
#include "monday/core/errors.h"
#include "monday/inference/IInferenceEngine.h"
#include "src/generated/blackboard_accessors.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

using namespace monday;

// ── Internal state ────────────────────────────────────────────────────────────

static IInferenceEngine* s_engine     = nullptr;
static MondayLoopConfig  s_cfg        = {};
static bool              s_running    = false;

// ── Turn lifecycle stubs (Phases 2–4 will fill these) ─────────────────────────

static void turn_begin_stub(void) {
    // Phase 1: reset PU_LEVEL arena
    monday_level_reset();
}

static void turn_end_stub(void) {
}

// ── Streaming token accumulator ───────────────────────────────────────────────

struct StreamAccum {
    char   buf[65536];
    size_t len;
    bool   cancelled;
};

// ── Public API ────────────────────────────────────────────────────────────────

void monday_loop_init(monday::IInferenceEngine *engine,
                      const MondayLoopConfig   &cfg) {
    s_engine  = engine;
    s_cfg     = cfg;
    s_running = false;
}

void monday_loop_run(void) {
    if (!s_engine) {
        monday_log_write(MONDAY_LOG_SYSTEM, "main_loop: no engine — cannot run");
        return;
    }

    s_running = true;
    char line_buf[8192];

    // Prompt to user
    if (s_cfg.text_only) {
        fprintf(stdout, "Monday (text-only mode). Type a message and press Enter.\n");
        fprintf(stdout, "Type 'exit' or Ctrl-D to quit.\n");
    }

    while (s_running) {
        if (s_cfg.text_only) {
            fprintf(stdout, "You: ");
            fflush(stdout);
        }

        if (!fgets(line_buf, sizeof(line_buf), stdin)) {
            // EOF or error — clean exit
            break;
        }

        // Strip trailing newline
        size_t len = strlen(line_buf);
        while (len > 0 && (line_buf[len-1]=='\n'||line_buf[len-1]=='\r')) {
            line_buf[--len] = '\0';
        }
        if (len == 0) continue;
        if (strcmp(line_buf, "exit") == 0 || strcmp(line_buf, "quit") == 0) break;

        // ── Turn begin ────────────────────────────────────────────────────────
        turn_begin_stub();

        // ── Build inference request ───────────────────────────────────────────
        InferenceRequest req = {};
        req.prompt         = line_buf;
        req.max_new_tokens = s_cfg.max_new_tokens ? s_cfg.max_new_tokens : 512;
        req.temperature    = s_cfg.temperature;
        req.top_p          = s_cfg.top_p;
        req.append_to_kv   = true;  // maintain conversational context

        if (s_cfg.text_only) {
            fprintf(stdout, "Monday: ");
            fflush(stdout);
        }

        // ── Streaming inference ───────────────────────────────────────────────
        StreamAccum accum = {};
        accum.len = 0;
        accum.cancelled = false;

        req.on_token = [&accum](int32_t tok_id, const char *tok_text) -> bool {
            if (tok_id == -1 || !tok_text) {
                accum.cancelled = true;
                return false;
            }
            size_t tlen = strlen(tok_text);
            if (accum.len + tlen + 1 < sizeof(accum.buf)) {
                memcpy(accum.buf + accum.len, tok_text, tlen);
                accum.len += tlen;
                accum.buf[accum.len] = '\0';
            }
            fputs(tok_text, stdout);
            fflush(stdout);
            return true;
        };

        auto result = s_engine->run_inference(req);

        // Print newline after streaming output
        fprintf(stdout, "\n");
        fflush(stdout);

        if (!result) {
            monday_log_write(MONDAY_LOG_DIAGNOSTIC, "main_loop: inference error");
            fprintf(stdout, "[Error: inference failed]\n");
        }

        // ── Turn end ──────────────────────────────────────────────────────────
        turn_end_stub();

        // ── Benchmark output ──────────────────────────────────────────────────
        if (s_cfg.benchmark_mode && result) {
            fprintf(stdout, "[tokens=%u, truncated=%s]\n",
                    result->token_count,
                    result->was_truncated ? "yes" : "no");
        }
    }

    s_running = false;
    monday_log_write(MONDAY_LOG_SYSTEM, "main_loop: exiting");
}

void monday_loop_shutdown(void) {
    s_running = false;
}

extern "C" void monday_loop_shutdown_c(void) {
    monday_loop_shutdown();
}
